package com.gta.engine;

import android.app.Activity;
import android.os.Bundle;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.MotionEvent;

public class GTAEngine extends Activity implements SurfaceHolder.Callback {
    static { System.loadLibrary("gta_engine"); }

    native void nativeInit(Surface surface, String dataPath);
    native void nativeRender();
    native void nativeTouch(int action, float x, float y);

    private SurfaceView surfaceView;
    private boolean running = false;
    private Thread renderThread;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        surfaceView = new SurfaceView(this);
        surfaceView.getHolder().addCallback(this);
        setContentView(surfaceView);
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        nativeTouch(e.getAction(), e.getX(), e.getY());
        return true;
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        String dataPath = getExternalFilesDir(null).getAbsolutePath();
        nativeInit(holder.getSurface(), dataPath);
        running = true;
        renderThread = new Thread(() -> {
            while(running) nativeRender();
        });
        renderThread.start();
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        running = false;
        try { renderThread.join(); } catch(Exception e) {}
    }

    @Override
    public void surfaceChanged(SurfaceHolder h, int f, int w, int t) {}
}
