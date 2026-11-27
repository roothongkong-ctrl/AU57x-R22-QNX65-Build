from pathlib import Path
import sys
from playwright.sync_api import sync_playwright

NAME = "MHI2_CN_AU57x_K1006_2_MU1005_4M0906961EJ.7z"
PAGE = "https://mibvault.com/files/MHI2%20-%20HARMAN/Firmware/Audi/CN"
dest = Path(sys.argv[1])
with sync_playwright() as p:
    browser = p.chromium.launch(headless=True)
    page = browser.new_page(accept_downloads=True)
    response = page.goto(PAGE, wait_until="domcontentloaded", timeout=60000)
    if (response and response.status in (401,403,429)) or "Security Checkpoint" in page.title():
        sys.exit("MIRROR_ACCESS_RESTRICTED: stop automated retrieval; normal authorized download required")
    label = page.get_by_text(NAME, exact=False).first
    try:
        label.wait_for(timeout=30000)
    except Exception:
        print("MIRROR_PAGE_TITLE", page.title(), "STATUS", flush=True)
        print("MIRROR_FILENAME_PRESENT", NAME in page.locator("body").inner_text()[:200000], flush=True)
        raise
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
