#!/bin/bash
set -e

PROJECT=$(pwd)
BUILD=$PROJECT/build
TOOLZ=$PROJECT/toolz
ANDROID_JAR=$TOOLZ/android.jar
PKG=com.gta.engine

# Usar aapt2 del sistema o del PATH
AAPT2=${AAPT2:-aapt2}
D8=${D8:-d8}
APKSIGNER=${APKSIGNER:-apksigner}
JAVAC=${JAVAC:-javac}

mkdir -p $BUILD/obj $BUILD/gen $BUILD/apk

echo "=== Compilando recursos ==="
$AAPT2 compile \
    $PROJECT/app/src/main/res/layout/activity_main.xml \
    -o $BUILD/obj/

echo "=== Linking APK ==="
$AAPT2 link \
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
$JAVAC -source 8 -target 8 \
    -classpath $ANDROID_JAR \
    -d $BUILD/obj \
    $BUILD/gen/com/gta/engine/R.java \
    $PROJECT/app/src/main/java/com/gta/engine/*.java

echo "=== Dex ==="
$D8 --output $BUILD/apk/ $BUILD/obj/com/gta/engine/*.class

echo "=== Agregando dex y libs al APK ==="
cd $BUILD/apk
zip unsigned.apk classes.dex

# Agregar .so si existe
if [ -d "$PROJECT/app/src/main/jniLibs" ]; then
    cd $PROJECT
    mkdir -p lib/arm64-v8a && cp app/src/main/jniLibs/arm64-v8a/libgta_engine.so lib/arm64-v8a/ && zip -r $BUILD/apk/unsigned.apk lib/
fi

echo "=== Firmando ==="
if [ ! -f $TOOLZ/debug.keystore ]; then
    keytool -genkeypair -v -keystore $TOOLZ/debug.keystore \
        -alias debug -keyalg RSA -keysize 2048 -validity 10000 \
        -dname "CN=Debug,O=GTA,C=MX" -storepass android -keypass android
fi

$APKSIGNER sign \
    --ks $TOOLZ/debug.keystore \
    --ks-pass pass:android \
    --key-pass pass:android \
    --out $BUILD/apk/GTAEngine.apk \
    $BUILD/apk/unsigned.apk

echo "=== APK listo ==="
ls -lh $BUILD/apk/GTAEngine.apk
