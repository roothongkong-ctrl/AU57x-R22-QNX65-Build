MIB Cluster Map v1.2.14 exact upstream source patch (NO INSTALL)

This does not modify the supplied SD ZIP. It is a source-review prototype requiring a separate live original-card RGBA producer that does not yet exist on the AU57x K1006_2. The upstream receiver currently handles only one type-111 cluster stream and map/card are alternative URLs, not simultaneous feeds. Apple SDK describes a type-112 second-cluster stream, but it is unverified on this car and must not be enabled by guesswork.

Changes: three added lines in player/opengl_gpu.cc, plus one new optional GPU layer header player/compact_card_layer.h. The layer is OFF unless CLUSTERMAP_CARD_INPUT is set to a valid /tmp/ file, and rejects files older than 400ms. It draws the original alpha-prepared card at 70 percent above the unchanged map, using GLES2. It does not get card video, decode second stream, or derive transparency from H264. Because these pieces are missing, do not install. The GitHub workflow checks strict patch applicability, host source compilation and repeated sanitizer stress tests; neither real iPhone nor QNX ARM execution is verified.

User original ZIP hash SHA256 7450ac0761d97a8de05e74c961c9e44e619c279fb2faa5e7683098de115485a1.
