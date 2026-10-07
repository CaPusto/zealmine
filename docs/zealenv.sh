# Zeal 8-bit host-mode toolchain environment
# usage: source ~/work/zealenv.sh

export ZOS_PATH="$HOME/work/sdk/Zeal-8-bit-OS"
export ZVB_SDK_PATH="$HOME/work/sdk/Zeal-VideoBoard-SDK"
export ZGDK_PATH="$HOME/work/sdk/zeal-game-dev-kit"
export COREUTILS_PATH="$HOME/work/sdk/zeal-coreutils"

export SDCC_PATH="$HOME/work/tools/sdcc-4.4.0"
export CMAKE_PATH_DIR="$HOME/work/tools/cmake-3.31.6-linux-x86_64"

export PYENV_BIN="$HOME/work/tools/pyenv/bin"
export PYTHON_BIN="$PYENV_BIN/python"

export PATH="$PYENV_BIN:$SDCC_PATH/bin:$ZOS_PATH/tools:$ZVB_SDK_PATH/tools/zeal2gif:$ZVB_SDK_PATH/tools:$CMAKE_PATH_DIR/bin:$PATH"

export ZEAL_NATIVE="$HOME/work/zeal-native-linux-x64"
export PATH="$ZEAL_NATIVE:$PATH"