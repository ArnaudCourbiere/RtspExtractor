package me.courbiere.rtspsamplesource;

public class TrackInfo {
    public final String mimeType;
    public final long duration;
    public TrackInfo(String mimeType, long duration) {
        this.mimeType = mimeType;
        this.duration = duration;
    }
}
