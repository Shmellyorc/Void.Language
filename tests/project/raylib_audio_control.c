// Native allocation control: no VOID, provider, callback adapter or test harness.
#include <raylib.h>
#include <stdio.h>
#include <string.h>
int main(int argc, char** argv) {
    setbuf(stdout,NULL);
    InitAudioDevice();
    if(!IsAudioDeviceReady())return 2;
    unsigned char data[8044]={82,73,70,70,100,31,0,0,87,65,86,69,102,109,116,32,16,0,0,0,1,0,1,0,64,31,0,0,64,31,0,0,1,0,8,0,100,97,116,97,64,31,0,0};
    memset(data+44,128,8000);
    if(argc>1 && strcmp(argv[1],"--alias")==0) {
        Wave wave=LoadWaveFromMemory(".wav",data,sizeof data);
        Sound sound=LoadSoundFromWave(wave);UnloadWave(wave);
        Sound alias=LoadSoundAlias(sound);UnloadSoundAlias(alias);UnloadSound(sound);
    } else if(argc>1 && strcmp(argv[1],"--music")==0) {
        Music music=LoadMusicStreamFromMemory(".wav",data,sizeof data);
        if(!IsMusicValid(music))return 3;
        PlayMusicStream(music);UpdateMusicStream(music);UnloadMusicStream(music);
    }
    CloseAudioDevice();
    puts("native control: audio terminal releases completed");
    return 0;
}
