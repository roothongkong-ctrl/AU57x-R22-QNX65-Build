from pathlib import Path
import sys
from playwright.sync_api import sync_playwright

NAME = "MHI2_CN_AU57x_K1006_2_MU1005_4M0906961EJ.7z"
PAGE = "https://mibvault.com/files/MHI2%20-%20HARMAN/Firmware/Audi/CN"
dest = Path(sys.argv[1])
with sync_playwright() as p:
    browser = p.chromium.launch(headless=True)
    page = browser.new_page(accept_downloads=True)
    page.goto(PAGE, wait_until="domcontentloaded", timeout=60000)
    label = page.get_by_text(NAME, exact=True).first
    label.wait_for(timeout=30000)
    # Select only the nearest card containing this exact filename and a single Download control.
    card = label
    control = None
    for _ in range(7):
        for role in ("button", "link"):
            matches = card.get_by_role(role, name="Download", exact=True)
            if matches.count() == 1:
                control = matches.first
                break
        if control is not None:
            break
        card = card.locator("..")
    if control is None:
        sys.exit("MIRROR_DOWNLOAD_CONTROL_NOT_FOUND")
    print("MIRROR_EXACT_FILENAME_FOUND", flush=True)
    with page.expect_download(timeout=180000) as info:
        control.click(timeout=30000)
    d = info.value
    if d.suggested_filename != NAME:
        d.cancel()
        sys.exit("MIRROR_UNEXPECTED_FILENAME")
    d.save_as(str(dest))
    browser.close()
print("MIRROR_DOWNLOAD_COMPLETE", dest.stat().st_size)
