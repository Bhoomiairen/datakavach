// Fonts are bundled with the app (no Google Fonts request): it works offline, and nothing leaves the machine.
import "@fontsource-variable/fraunces/full.css";
import "@fontsource/ibm-plex-sans/400.css";
import "@fontsource/ibm-plex-sans/500.css";
import "@fontsource/ibm-plex-sans/600.css";
import "@fontsource/ibm-plex-mono/400.css";
import "@fontsource/ibm-plex-mono/500.css";
import "@fontsource/ibm-plex-mono/600.css";
import "@fontsource/ibm-plex-mono/700.css";
import "@fontsource/tiro-devanagari-hindi/400.css";
import "./globals.css";
import Masthead from "@/components/Masthead";

export const metadata = {
  title: "DataKavach",
  description: "Finds and hides Indian personal data — Aadhaar, PAN, GSTIN, UPI, phone — in files and AI prompts.",
  icons: { icon: "/favicon.svg" },
};

export const viewport = { width: "device-width", initialScale: 1, themeColor: "#f4efe4" };

export default function RootLayout({ children }) {
  return (
    <html lang="en">
      <body>
        <Masthead />
        <main>{children}</main>
        <footer className="colophon">
          <span>DataKavach · a student project by Bhoomi Airen · synthetic data only, never real people</span>
          <span>C++ engine · Next.js · runs on your own machine</span>
        </footer>
      </body>
    </html>
  );
}
