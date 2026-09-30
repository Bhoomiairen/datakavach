import ScanReport from "@/components/ScanReport";

export const metadata = { title: "Scan files · DataKavach" };

export default function ScanPage() {
  return (
    <>
      <div className="page-head">
        <div>
          <span className="kicker">Data discovery</span>
          <h1>Which files hold personal data?</h1>
          <p className="lede">
            India’s DPDP Act expects a company to know where personal data sits. DataKavach walks a folder on
            every CPU core, and reports it by file and by spreadsheet column — without copying a single raw value into the report.
          </p>
        </div>
      </div>
      <ScanReport />
    </>
  );
}
