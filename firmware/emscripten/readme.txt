get_em              [runs [PATH]/emscripten/emsdk/emsdk_env.sh]
cd build
emcmake cmake ..    [first time or when files are added or have been deleted]
emmake make -j4
gzip index.html
mv index.html.gz ../../components/esp32_web
