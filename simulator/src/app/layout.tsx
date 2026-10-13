import type { Metadata } from "next";
import { Bebas_Neue, IBM_Plex_Mono, Source_Sans_3 } from "next/font/google";
import "./globals.css";

const bebas = Bebas_Neue({
  weight: "400",
  subsets: ["latin"],
  variable: "--font-bebas",
});

const plexMono = IBM_Plex_Mono({
  weight: ["400", "500", "600", "700"],
  subsets: ["latin"],
  variable: "--font-ibm-plex-mono",
});

const sourceSans = Source_Sans_3({
  subsets: ["latin"],
  variable: "--font-source-sans",
});

export const metadata: Metadata = {
  title: "SmartPendant 7″ Portrait Lab",
  description:
    "Layout lab for the SmartPendant fork targeting a 7″ 480×800 portrait touch screen.",
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="en">
      <body
        className={`${bebas.variable} ${plexMono.variable} ${sourceSans.variable} min-h-screen antialiased`}
      >
        {children}
      </body>
    </html>
  );
}
