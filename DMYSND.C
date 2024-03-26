#include "sb.h"

int InitSound(int Mode)                 /* Initialize sound */
    /* Returns 1 on success, 0 otherwise. Mode=0 to skip     */
    /* initialization (will be silent), Mode=1 to use Adlib, */
    /* Mode=2..7 to use SoundBlaster wave synthesis.         */
{
	return 1;
}
void TrashSound(void)                   /* Shut down sound  */
{
}
void SetSound(int Channel,int Type)      /* Set sound type   */
{
}
void SetFreq(int Channel,int Freq)       /* Set frequency    */
{
}
void SetVolume(int Channel,int Volume)   /* Set volume       */
{
}
void SetChannels(int Volume,int Toggle)
    /* Set master volume (0..255) and turn channels on/off.  */
    /* Each bit in Toggle corresponds to a channel (1=on).   */
{
}
void Sound(int Channel,int Freq,int Volume)
    /* Generate sound of given frequency (Hz) and volume     */
    /* (0..255) via given channel.                           */
{
}
