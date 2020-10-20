/* Jean-Roch Lauper, October 2020
 * 
 * very slights modifications:
 * 
 *  (1) some "static"s added to avoid multiple definition with functions with same 
 *      names in other files (especially e_freq_recognition.c/h that reuses
 *      a lot of the functions originally developed in this file)
 * 
 *  (2) while(1) replaced by a loop in the main.c file 
 *      to enable remote control usage (among others to stop the running)
 *
 *  (3) -change of the necessary volume to be taken into account
 *      -change of the frequencies (too high to whistle easily)
 *      -adding a 4th tone to stop
 * 
 */

/********************************************************************************

			Programm to demonstrate how FFT works				          
			Version 2.0 ao�t 2007				                          
			Michael Bonani, Jonathan Besuchet


This file is part of the e-puck library license.
See http://www.e-puck.org/index.php?option=com_content&task=view&id=18&Itemid=45

(c) 2004-2007 Michael Bonani, Jonathan Besuchet

Robotics system laboratory http://lsro.epfl.ch
Laboratory of intelligent systems http://lis.epfl.ch
Swarm intelligent systems group http://swis.epfl.ch
EPFL Ecole polytechnique federale de Lausanne http://www.epfl.ch

**********************************************************************************/

/*! \file
 * \brief The frequency recognizer using FFT
 * \section sect1 Introduction
 * The runfftlistener programm is made to illustrate how you can use the
 * FFT package of the library. The goal is to determine the frequency of
 * the sound comming from the microphone number 0. If the frequency is 
 * under 900Hz the e-puck will turn left. If the frequency is between
 * 900Hz and 1800Hz the e-puck will go forward. If the frequency is over
 * 1800Hz the e-puck will turn right.
 * 
 * \section sect2 Playing the demo
 * First of all, move the selector to the position 9 to 15 and reset the e-puck.
 * To play the demo, you have two allternatives:
 * - You are a good whistler. In this case you can drive the e-puck with
 * your mouse by whistling in the good frequency.
 * - You do not know whistlering. In this case you can play the "sound1.mp3"
 * or "sound2.mp3" which are in the folder "demo" to drive the e-puck.
 *
 * \section sect3 Video of the demo
 * - Driving the e-puck by whistlering and playing sound on PC: http://www.youtube.com/watch?v=bfHFo79uZGY
 *
 * \author Code: Michael Bonani, Jonathan Besuchet \n Doc: Jonathan Besuchet
 */

#include <p30Fxxxx.h>
#include <dsp.h>

#include "math.h"
#include "stdbool.h"

//#include "motor_led/e_epuck_ports.h"
//#include "motor_led/e_init_port.h"
//#include "a_d/advance_ad_scan/e_ad_conv.h"
//#include "fft/e_fft.h"
//#include "fft/e_fft_utilities.h"
//#include "motor_led/advance_one_timer/e_motors.h"
//#include "motor_led/advance_one_timer/e_agenda.h"
//#include "motor_led/advance_one_timer/e_led.h"

#include "e_epuck_ports.h"
#include "e_init_port.h"
#include "e_ad_conv.h"
#include "e_fft.h"
#include "e_fft_utilities.h"
#include "e_motors.h"
#include "e_agenda.h"
#include "e_led.h"


/* Extern definitions */
/* D�finitions externes des variables globales, des diff�rents tableaux ou seront stock�s les signaux des diff�rents micros et la FFT du micro choisi, et
d�finitions de fonctions utiles de moyennage et de copie d'un buffer � l'autre*/ 
/* Typically, the input signal to an FFT routine is a complex array containing samples of an input signal. */
/* For this example, we will provide the input signal in an array declared in Y-data space. */
extern fractcomplex sigCmpx[FFT_BLOCK_LENGTH] __attribute__ ((section (".ydata, data, ymemory"),aligned (FFT_BLOCK_LENGTH * 2 *2)));      		
/* Access to the mic. samples */
extern int e_mic_scan[3][FFT_BLOCK_LENGTH];


/* JRL Volume threshold values
 * used in display_volume_on_led(int volume) */
// int vol_display_thresh[5] = {5,20,50,200,300}; // original - too high from experience
int vol_display_thresh[5] = {25,50,75,100,125}; // new one /more sensitive


/*! \brief Localize the bigger pic of the array
 * \param spectre The array in which the FFT was made
 * \param spectre_length The length of the scan in the array
 * \return The index of the bigger pic detected
 */
static int localise_pic_max(fractcomplex *spectre, int spectre_length)
{			
	int i = 0 ;
	long ampl_max= 0 ;	// Initialisation de l'amplitude maximale
	long ampl_courante = 0 ;  //Initialisation de l'amplitude courante
	int pic_max;
	for (i = 0; i < spectre_length/2; i++)	
	{
		ampl_courante = spectre[i].real*spectre[i].real+spectre[i].imag*spectre[i].imag ; // Calcul de l'amplitude de la FFT � la position i courante
		
		if (ampl_courante>ampl_max)						// Si l'amplitude courante est plus grande que l'amplitude maximale m�moris�e jusqu'ici...
			{
				pic_max = i ;							// La position du Maxima est m�moris�e dans k_max_1
				ampl_max = ampl_courante ;				// La valeur de l'amplitude maximum est remplac�e par la valeur courante
			}
	}
	return pic_max;
}

/*! \brief Get the max volume of the sound detected
 * \param spectre The array in which the FFT was made
 * \param pic_pos The index of the louder frequency
 * \return The amplitude of the louder frequency
 */
static int get_volume(fractcomplex *spectre, int pic_pos)
{
	if(pic_pos < 0 || pic_pos >= FFT_BLOCK_LENGTH/2)
		return 0;
	return spectre[pic_pos].real*spectre[pic_pos].real+spectre[pic_pos].imag*spectre[pic_pos].imag;
}

/*! \brief Display the volume of the soud detected on the LEDs
 * \param volume The max volume of the sound detected
 */
// JRL modification with volume threshold coming from an array 
// more easily modifiable
static void display_volume_on_led(int volume)
{
	e_led_clear();
	if(volume > vol_display_thresh[0] && volume < vol_display_thresh[1])
		e_set_led(4, 1);
	else if(volume >= vol_display_thresh[1] && volume < vol_display_thresh[2])
	{
		e_set_led(4, 1);
		e_set_led(3, 1);
		e_set_led(5, 1);
	} else if(volume >= vol_display_thresh[2] && volume < vol_display_thresh[3])
	{
		e_set_led(4, 1);
		e_set_led(3, 1);
		e_set_led(5, 1);
		e_set_led(2, 1);
		e_set_led(6, 1);
	} else if(volume >= vol_display_thresh[3] && volume < vol_display_thresh[4])
	{
		e_set_led(4, 1);
		e_set_led(3, 1);
		e_set_led(5, 1);
		e_set_led(2, 1);
		e_set_led(6, 1);
		e_set_led(1, 1);
		e_set_led(7, 1);
	} else if(volume >= vol_display_thresh[4])
	{
		e_set_led(8, 1);
	}
}

/*! \brief Calcul the corresponding frequency of the bigger pic detected
 * \param pic_pos The index of the bigger pic detected
 * \return The corresponding frequency
 */
static int calcul_frequence(int pic_pos)
{
	return (pic_pos*33000)/FFT_BLOCK_LENGTH;
}

/*! \brief Set the speed of the e-puck
 *
 * Set the speed of the e-puck relatively of the frequency
 * of the sound detected.
 * - Turn left if the frequency is under 900Hz
 * - Turn right if the frequency is over 1800Hz
 * - Go forward if the sound is between 900Hz and 1800Hz
 *
 * \param frequency The frequence of the sound
 */
static void set_speed(int frequency)
{
    // ***** original code *****
//	if(frequency < 900)
//		e_set_speed(300, 200);
//	else if(frequency > 1800)
//		e_set_speed(300, -200);
//	else
//		e_set_speed(400, 0);
    
    // ***** jrl mod *****
    if(frequency < 905) 
        /* because of fft : returnable values: 902, 1031...
        900 not a good choice 
        "goal tone" : G5: 783.99 Hz / recognized 773 Hz */
        e_set_speed(300,200);
    
    else if (frequency < 1300)
        /* fft returnable:1031,1160,1289, ...
         * "goal tone": C6: 1046.5 / recognized 1031 Hz */
        e_set_speed(300,-200);
    
    else if (frequency < 2050)
        /* "goal tone": G6: 1567.98 Hz / recognized 1546 Hz*/
        e_set_speed(400,0);
    
    else if (frequency < 2900)
        /* harmonics very high with speakers (14'000...)
         * Filter to remove high frequencies */
        e_set_speed(0,0);                       
    
}


/* JRL mods for externalized main loop -> enables remote control use to stop the program */
static bool first_run = true;



/*! \brief The "main" function of the demo */
void run_fft_listener(void)
{
	int volume;
	int pos_pic;
	int frequency;

    
    if (first_run) {
        // Initialisations
        //	e_init_port(); // JRL already in main.c
        //  e_init_motors(); // JRL already in main.c
        //  e_start_agendas_processing(); // JRL already in main.c
        e_init_ad_scan(MICRO_ONLY); 
            //JRL necessary as in main.c e_init_ad_scan(ALL_ADC) by default
        e_set_speed(400, 0);
        first_run = false;
    }
		

//	while(1) // -> in the main loop
//	{
		// Scanage des microphone	
		e_ad_scan_on();

		// Attente de l'acquisition de toute les valeurs
		while(!e_ad_is_array_filled());
		e_ad_scan_off();

		// Centrage du signal au point z�ro (moyenne = 0)
		e_subtract_mean(e_mic_scan[0], FFT_BLOCK_LENGTH, LOG2_BLOCK_LENGTH);

		// Copie le signal du micro zero dans le buffer destin� � effectuer la FFT
		// Affecte les valeurs � sigCmpx.real (r�els) et 0 � sigCmpx.imag (imaginaires)
		e_fast_copy(e_mic_scan[0], (int*)sigCmpx, FFT_BLOCK_LENGTH);
		
		// Le r�sultat est sauvegard� => On d�marre une nouvelle acquisition
		e_ad_scan_on();

		// Execution de la FFT sur le buffer
		e_doFFT_asm(sigCmpx);

		// Recherche de la position K des deux fr�quences maximum 				
		pos_pic = localise_pic_max(sigCmpx, FFT_BLOCK_LENGTH);
					
		volume = get_volume(sigCmpx, pos_pic);
		display_volume_on_led(volume);
//		if(volume > 100) // JRL too high for a nice workable demo
        if(volume > 15)
		{
			frequency = calcul_frequence(pos_pic);
			set_speed(frequency);
		}
//	}		
}
