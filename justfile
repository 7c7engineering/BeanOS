
build:
    git submodule update --init --recursive
    idf.py build


setup-lsp:
    #!/usr/bin/env bash
    LATEST_ESP_VERSION=$(ls -r $HOME/.espressif/tools/xtensa-esp-elf | head -1)
    XTENSA_PATH="$HOME/.espressif/tools/xtensa-esp-elf/$LATEST_ESP_VERSION/xtensa-esp-elf"
    ESP_VERSION=$(ls -r $XTENSA_PATH/lib/gcc/xtensa-esp-elf | head -1)

    echo "# Based on https://github.com/espressif/esp-idf/issues/6721" > .clangd
    echo "CompileFlags:" >> .clangd
    echo "  Add: [" >> .clangd
    echo "    -isystem," >> .clangd
    echo "    $XTENSA_PATH/lib/gcc/xtensa-esp-elf/$ESP_VERSION/include-fixed," >> .clangd
    echo "    -isystem," >> .clangd
    echo "    $XTENSA_PATH/xtensa-esp-elf/include/," >> .clangd
    echo "  ]" >> .clangd
    echo "  Remove: [" >> .clangd
    echo "    -fno-shrink-wrap," >> .clangd
    echo "    -fno-tree-switch-conversion," >> .clangd
    echo "    -fstrict-volatile-bitfields," >> .clangd
    echo "    -mdisable-hardware-atomics" >> .clangd
    echo "  ]" >> .clangd
    echo "  CompilationDatabase: build" >> .clangd

