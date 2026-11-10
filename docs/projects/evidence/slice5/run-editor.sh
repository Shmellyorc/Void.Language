#!/bin/bash
set -e
export PATH="/home/shmellyorc/.nvm/versions/node/v24.19.0/bin:$PATH"
export VSCODE_EXECUTABLE_PATH="$PWD/editors/vscode/.vscode-test/vscode-linux-x64-1.141.0/code"
export VOIDLS_PATH="$PWD/build/llvm23-release/voidls" VOID_CREATION_EVIDENCE_DIR="$PWD/build/slice5"
export VOID_OLDER_VOIDLS_PATH="$PWD/build/slice3-older-voidls"
cd editors/vscode
npm run build
node out/test/runCreationHost.js
