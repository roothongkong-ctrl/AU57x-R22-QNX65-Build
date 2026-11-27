# Compact Original Card R1 – source-only GitHub experiment

**Target**: Audi A6L C7.5, Chinese MHI2_CN_AU57x_K1006_2 / MU1005; instrument map 800×298.

This experiment preserves the iPhone's **original card pixels** and shrinks the card to 70% while preserving all map pixels **outside the new reduced card rectangle**. It is a reproducible dual-input simulator and **NOT an installable QNX patch**. No manufacturer binaries, secrets, keys or firmware are included.

## Caveat / next gate

Upstream [MacsInSpace/mib-cluster-map](https://github.com/MacsInSpace/mib-cluster-map/blob/v1.2.14/thirdparty/mhi2_altscreen_carplay/libaltscreen111_gen2.c) supports alternative Type-111 initialURL values: full/map/instructioncard. The single active Type-111 receiver listens on tcp://127.0.0.1:19820. **Alternative URLs are not simultaneous streams.** The two concurrent picture inputs necessary for real, pixel-preserving resizing have not been demonstrated on this Audi / AMap. If only the already-blended image exists, nothing can recreate road pixels hidden underneath the original large opaque card. Do not install or claim CarPlay compatibility until source acquisition is solved.

The module is new MIT-licensed prototype code, not a copy of upstream GPL code. This isolated experimental branch avoids changing the separate Wireless CarPlay main branch.

## Run

From this directory:

~~~bash
python3 -m pip install Pillow
g++ -std=c++11 -O2 -Wall -Wextra -Werror -pedantic -o card_test test_compose.cpp
./card_test
g++ -std=c++11 -O2 -Wall -Wextra -Werror -pedantic -o card_demo demo.cpp
python3 make_mock.py
./card_demo mock/map.rgba mock/card.rgba mock/after70.rgba 800 298 16 18 338 126 70
./card_demo mock/map.rgba mock/card.rgba mock/after100.rgba 800 298 16 18 338 126 100
python3 render_preview.py
~~~

GitHub Actions creates **ClusterCard-R1-SIMULATION-NO-INSTALL** with before/after screenshots, JSON report, and tests. Real CarPlay dual-source/decoder/QNX compilation is future work.
