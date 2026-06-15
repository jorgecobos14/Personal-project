#!/bin/bash
set -e

PROJECT=~/GTAEngine
BUILD=$PROJECT/build
TOOLZ=$PROJECT/toolz
ANDROID_JAR=$TOOLZ/android.jar
PKG=com.gta.engine
APP_NAME="GTAEngine"

mkdir -p $BUILD/obj $BUILD/gen $BUILD/apk

echo "=== Compilando recursos ==="
aapt2 compile \
    $PROJECT/app/src/main/res/layout/activity_main.xml \
    -o $BUILD/obj/

echo "=== Linking APK ==="
aapt2 link \
    -o $BUILD/apk/unsigned.apk \
    -I $ANDROID_JAR \
    --manifest $PROJECT/app/src/main/AndroidManifest.xml \
    --java $BUILD/gen \
    $BUILD/obj/layout_activity_main.xml.flat \
    --min-sdk-version 26 \
    --target-sdk-version 34 \
    --version-code 1 \
    --version-name "0.1"

echo "=== Compilando Java ==="
javac -source 8 -target 8 \
    -classpath $ANDROID_JAR \
    -d $BUILD/obj \
    $BUILD/gen/$PKG/R.java \
    $PROJECT/app/src/main/java/$PKG/*.java

echo "=== Dex ==="
d8 --output $BUILD/apk/ $BUILD/obj/**/*.class

echo "=== Agregando dex al APK ==="
cd $BUILD/apk && zip unsigned.apk classes.dex

echo "=== Compilando C++ ==="
# TODO: compilar .so con clang

echo "=== Firmando ==="
if [ ! -f $TOOLZ/debug.keystore ]; then
    keytool -genkeypair -v -keystore $TOOLZ/debug.keystore \
        -alias debug -keyalg RSA -keysize 2048 -validity 10000 \
        -dname "CN=Debug,O=GTA,C=MX" -storepass android -keypass android
fi

apksigner sign \
    --ks $TOOLZ/debug.keystore \
    --ks-pass pass:android \
    --key-pass pass:android \
    --out $BUILD/apk/GTAEngine.apk \
    $BUILD/apk/unsigned.apk

echo "=== APK listo ==="
ls -lh $BUILD/apk/GTAEngine.apk
cp $BUILD/apk/GTAEngine.apk ~/storage/downloads/
echo "Copiado a Downloads"
