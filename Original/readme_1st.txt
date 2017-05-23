README for the WOLF GUI Project
--------------------------------------------------------------
  Last modified: 2013-02-16 (YYYY-MM-DD)
  Website: http://www.qsl.net/d/dl4yhf/wolf/index.html


  Please read the DISCLAIMER at the end of this
  document before using this software !


What is WOLF ?
- - - - - - - - - - - - - - - - - - - -

  WOLF was written by Stewart Nelson (KK7KA). It means 
  "Weak-signal Operation on Low Frequency". 
  WOLF can operate over a wide range of signal levels. 
  For example, a WOLF beacon transmits a 15-character 
  message repeatedly. If the received signal would be 
  adequate for conventional CW, copy will be displayed
  in 24 seconds.
  More background information can be found 
  on Stewart Nelson's website :
     http://www.scgroup.com/ham/wolf.html 


What is the WOLF GUI ?
- - - - - - - - - - - - - - - - - - - -

GUI means "graphic user interface" in this case. 
The original WOLF implementation by KK7KA was a simple 
WIN32 console application. I (DL4YHF) tried to write 
a user interface for it in April 2005, using Borland 
C++Builder V4. At the moment, this is still 
"under construction", so please don't expect too much. 
The plan is...

   1. First to write a simple user interface, 
       not too crowded but allows adjusting 
       the most important WOLF parameters,
       using the SAME sourcecodes in the WOLF console 
       application and the GUI variant to minimize 
       testing effort, and to simplify coordination 
       between co-workers (any takers ?..),
   2. implement real-time processing of data 
       from the soundcard ,
   3. implement some new features to simplify 
       bit-sync (possibly via GPS or other time 
       signal broadcasters) 

 Numbers 1 and 2 work to some extent,
  but number 3 still waits for a volunteer ..  ;-)

 ... but, before you start:

  - Read the disclaimer (at the end of this document) !
  - Read Stewart's original notes about WOLF !
  - If you will test WOLF on the air, please note 
    that the signal is not constant envelope, 
    and is relatively wide band (BW at -20 dB is about 40 Hz)
  - The authors will not be responsible for any 
    damage to your transmitter, nor for any QRM caused; 
    please examine the audio output before transmitting. 
  - Do not transmit with higher power than needed for your test.


Ok ? Then good luck on LF !

    Wolfgang ("Wolf") DL4YHF . 



What's new ?
- - - - - - - - - - - - - - - - - - - -
 2005-11-13: Tried different baudrates ("slow" and "fast").
             They can be changed through the "Mode"-menu .
 2005-11-14: Added the option to add "noise" to the
             receiver. Used for sensitivity tests .
 2005-11-20: Hopefully fixed a bug with the slowest mode
             which is now called "WOLF 2.5" which means
             2.5 symbols/second.
 2008-01-16: Added the option to select the audio input-
             and output device on the I/O Config tab .
             Earlier versions of the WOLF GUI always used
             the "default soundcard" on the PC .
 2012-02-16: Allowed new sampling rates like 12000 S/sec .
          



File List and installation
- - - - - - - - - - - - - - - - - - - -
The following files may be contained in the archive:
 - WOLF_gui.exe 
    Is all you need to RUN the WOLF GUI. The following
    sourcecodes are not required to run WOLF !
 - html\*.*
    A very preliminary document in HTML format :)

There is no installer for the WOLF GUI. To install,
simply copy everything from the zipped file into a 
directory of your own choice. Under Windows 7, it's
advisable NOT to install it under the windows 'Programs'
folder, because subdirectories in that folder are often
not writeable for the application, and thus the program
cannot store its own configuration in its 'own' file
(and this is what the WOLF GUI does: Store its configuration 
 data in its own directory, in a file named 'wolfcfg.ini').


The following files are only in the sourcecode archive:
 - WOLF.CPP, *.H
    Encoder- and decoder algorithms.
    Written by KK7KA, slightly modified by DL4YHF
 - WOLF_GUI1.CPP, *.H, *.DFM
    DL4YHF's first attempt for a graphic user interface
 - WolfThd.CPP, *.H
    WOLF worker threads. Used to let the WOLF core
    run in the background without blocking the GUI.
 - SoundMaths.CPP, *.H
    Some math subroutines for the GUI. Recycled from
    DL4YHF's "Spectrum Lab" and "Audio Utilities".
 - Sound.CPP, *.H
    Soundcard I/O routines
 - LinradColours.C
    Contains the colour palette for the waterfall
    (used in the 'tuning screen') .
 - WOLF_gui.BPR
    The Borland C++Builder File used by DL4YHF 
    to compile everything. Uses the original files .

    

Disclaimer
- - - - - - - - - - - - - - - - - - - - - - - - - - - -

 (Ok, we all hate this paranoid stuff, but 
      it may be wise to have it here .. )

 THIS SOFTWARE IS PROVIDED ''AS IS'' AND ANY EXPRESS 
 OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 SHALL THE AUTHOR AND CONTRIBUTORS BE LIABLE FOR ANY 
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, 
 OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
 CAUSED AND ON ANY THEORY OF LIABILITY, 
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED 
 OF THE POSSIBILITY OF SUCH DAMAGE.

Namings for products in the software and this manual,
that are registered trademarks, are not separately marked.
The same applies to copyrighted material. Therefore 
the missing (r) or (c) character does not implicate,
that the naming is a free trade name. 
Furthermore the used names do not indicate patent rights 
or anything similar.

WOLF was developed by Stewart Nelson (KK7KA). 
The user interface was written by Wolfgang Büscher (DL4YHF).

You may use this software, copy it, modify it, but only 
for non-profit purposes. The above copyright notice 
and terms of use shall be included in all copies or 
substantial portions of the software.

The program must not be used in 'critical' environments, 
where program failure may cause harm to persons or damage
to property. 
Dont rely on this program as an emergency communication medium !

Further restrictions to the use of WOLF may apply, 
please check Stewart Nelson's website too:
     http://www.scgroup.com/ham/wolf.html 

- - - - - - - - - - - - - - - - - - - - - - - - - - - -
