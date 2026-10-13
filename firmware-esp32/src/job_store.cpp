#include "job_store.h"
#include "pins_config.h"
#include "sample_job_nc.h"

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <SD.h>
#include <SPI.h>
#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static bool lfs_ok = false;
static bool sd_ok = false;
static SPIClass *sd_spi = nullptr;

static bool ends_with_nc(const char *name) {
  size_t n = strlen(name);
  if (n < 3) return false;
  const char *ext = name + n - 3;
  if (!strcasecmp(ext, ".nc")) return true;
  if (n >= 6 && !strcasecmp(name + n - 6, ".gcode")) return true;
  if (n >= 4 && !strcasecmp(name + n - 4, ".ngc")) return true;
  return false;
}

static void list_dir(fs::FS &fs, const char *dir, job_src_t src, job_entry_t *out,
                     unsigned max_out, unsigned *count) {
  File root = fs.open(dir);
  if (!root || !root.isDirectory()) return;
  File f = root.openNextFile();
  while (f && *count < max_out) {
    if (!f.isDirectory()) {
      const char *name = f.name();
      // ESP32 FS may return full path or basename
      const char *base = strrchr(name, '/');
      base = base ? base + 1 : name;
      if (ends_with_nc(base)) {
        job_entry_t *e = &out[*count];
        memset(e, 0, sizeof(*e));
        strncpy(e->name, base, sizeof(e->name) - 1);
        snprintf(e->path, sizeof(e->path), "%s/%s", dir, base);
        e->src = src;
        e->size = (uint32_t)f.size();
        (*count)++;
      }
    }
    f = root.openNextFile();
  }
}

void job_store_begin(void) {
  lfs_ok = LittleFS.begin(true);  // format on fail — first bring-up
  if (lfs_ok) {
    if (!LittleFS.exists("/jobs")) LittleFS.mkdir("/jobs");
    Serial.println("[jobs] LittleFS mounted");
  } else {
    Serial.println("[jobs] LittleFS mount failed");
  }

#if defined(JOB_STORE_SD) && JOB_STORE_SD
  // CrowPanel Advance TF: MOSI=6 MISO=4 SCK=5; CS often hard-tied to 3.3 V.
  // Requires rear DIP in a TF-card mode (shares pins with speaker I2S).
  if (!sd_spi) {
    sd_spi = new SPIClass(HSPI);
    sd_spi->begin(SD_SPI_SCK, SD_SPI_MISO, SD_SPI_MOSI, SD_SPI_CS >= 0 ? SD_SPI_CS : -1);
  }
  if (SD_SPI_CS >= 0) {
    sd_ok = SD.begin((uint8_t)SD_SPI_CS, *sd_spi);
  } else {
    // CS tied high — pass a dummy CS pin that we hold high; library needs a pin.
    pinMode(SD_SPI_DUMMY_CS, OUTPUT);
    digitalWrite(SD_SPI_DUMMY_CS, HIGH);
    sd_ok = SD.begin((uint8_t)SD_SPI_DUMMY_CS, *sd_spi);
  }
  if (sd_ok) {
    if (!SD.exists("/jobs")) SD.mkdir("/jobs");
    Serial.println("[jobs] SD mounted");
  } else {
    Serial.println("[jobs] SD not present (check DIP TF mode / card)");
  }
#else
  sd_ok = false;
  Serial.println("[jobs] SD disabled at build (JOB_STORE_SD=0) — LittleFS + embedded only");
#endif
}

bool job_store_lfs_ok(void) { return lfs_ok; }
bool job_store_sd_ok(void) { return sd_ok; }

unsigned job_store_list(job_entry_t *out, unsigned max_out) {
  if (!out || max_out == 0) return 0;
  unsigned n = 0;

  // Always expose the embedded sample first
  memset(&out[n], 0, sizeof(out[n]));
  strncpy(out[n].name, SAMPLE_JOB_NAME, sizeof(out[n].name) - 1);
  strncpy(out[n].path, "embedded:sample", sizeof(out[n].path) - 1);
  out[n].src = JOB_SRC_EMBEDDED;
  out[n].size = (uint32_t)strlen(SAMPLE_JOB_NC);
  n++;

  if (lfs_ok && n < max_out) {
    list_dir(LittleFS, "/jobs", JOB_SRC_LITTLEFS, out, max_out, &n);
  }
#if defined(JOB_STORE_SD) && JOB_STORE_SD
  if (sd_ok && n < max_out) {
    list_dir(SD, "/jobs", JOB_SRC_SD, out, max_out, &n);
  }
#endif
  return n;
}

static bool read_fs_file(fs::FS &fs, const char *path, char *buf, size_t buf_sz,
                         size_t *out_len) {
  File f = fs.open(path, FILE_READ);
  if (!f) return false;
  size_t avail = f.size();
  if (avail + 1 > buf_sz) {
    Serial.printf("[jobs] file too large (%u > %u)\n", (unsigned)avail, (unsigned)buf_sz);
    f.close();
    return false;
  }
  size_t got = f.read((uint8_t *)buf, avail);
  f.close();
  buf[got] = '\0';
  if (out_len) *out_len = got;
  return true;
}

bool job_store_read(const char *path, char *buf, size_t buf_sz, size_t *out_len) {
  if (!path || !buf || buf_sz < 2) return false;

  if (!strcmp(path, "embedded:sample") || !strncmp(path, "embedded:", 9)) {
    size_t n = strlen(SAMPLE_JOB_NC);
    if (n + 1 > buf_sz) return false;
    memcpy(buf, SAMPLE_JOB_NC, n + 1);
    if (out_len) *out_len = n;
    return true;
  }

  if (lfs_ok && read_fs_file(LittleFS, path, buf, buf_sz, out_len)) return true;
#if defined(JOB_STORE_SD) && JOB_STORE_SD
  if (sd_ok && read_fs_file(SD, path, buf, buf_sz, out_len)) return true;
#endif
  return false;
}

unsigned job_store_load_path(const char *path, path_seg_t *segs, unsigned max_segs,
                             char *name_out, size_t name_sz) {
  if (!path || !segs || max_segs == 0) return 0;

  // Prefer PSRAM for large NC buffers
  const size_t kBuf = 96 * 1024;
  char *buf = (char *)heap_caps_malloc(kBuf, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!buf) buf = (char *)malloc(kBuf);
  if (!buf) {
    Serial.println("[jobs] NC buffer alloc failed");
    return 0;
  }

  size_t len = 0;
  bool ok = job_store_read(path, buf, kBuf, &len);
  if (!ok) {
    free(buf);
    return 0;
  }

  unsigned count = gcode_parse_toolpath(buf, segs, max_segs);
  if (name_out && name_sz) {
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    if (!strncmp(path, "embedded:", 9)) base = SAMPLE_JOB_NAME;
    strncpy(name_out, base, name_sz - 1);
    name_out[name_sz - 1] = '\0';
  }
  Serial.printf("[jobs] loaded %s → %u segs (%u bytes)\n", path, count, (unsigned)len);
  free(buf);
  return count;
}
