#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
[[ "$(uname -s)" == Darwin ]] || exit 2
mkdir -p TestResults
xcodebuild -version | tee TestResults/xcode-version.txt
xcodebuild -showsdks | tee TestResults/sdks.txt
xcodegen generate
xcodebuild -project NASMusic.xcodeproj -scheme NASMusic -configuration Debug -sdk iphonesimulator -destination 'generic/platform=iOS Simulator' -derivedDataPath TestResults/DerivedData CODE_SIGNING_ALLOWED=NO build 2>&1 | tee TestResults/build.log
xcrun simctl list devices available -j > TestResults/devices.json
udid=$(python3 Scripts/select_simulator.py < TestResults/devices.json)
result="TestResults/simulator-$(date +%Y%m%d-%H%M%S).xcresult"
xcodebuild -project NASMusic.xcodeproj -scheme NASMusic -configuration Debug -destination "platform=iOS Simulator,id=$udid" -derivedDataPath TestResults/DerivedData -parallel-testing-enabled NO -resultBundlePath "$result" CODE_SIGNING_ALLOWED=NO test 2>&1 | tee TestResults/simulator-test.log
