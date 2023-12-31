//////////////////////////////////////////////////////////////
// Communication from the master is managed by the hardware.
// Communication from the following octaves is generated by SoftwareSerial.

// Alternatively, communication could be handled by the hardware, allowing the hardware to handle
// the intensive communication part, while the return to the master, which is simpler, is managed
// by the software.

// All messages end with '\n'.

// General reset: Received from the master: if the message starts with 'R', if it's a communication error,
// everything is reset. For example, if the master sends 'RRRRR', everything is guaranteed to be reset to 0,
// and a response of "R"+octaveNumber+"RR\n" is sent to the master to signal the reset.
// Assignment of the octave number: Received from the master: if the message starts with 'S', the number that
// follows is the current octave number (n), and 'S'+(n+1)+'\n' is transmitted, and 'S'+n+'\n' is sent back
// to the master.
// Request for the state of the keys: Received from the master: preceded by the octave number, followed by 'Q'.
// The response is a message in the form octaveNumber+'T'+state of the keys in hexadecimal in a string.
// Request to turn on a key: Received from the master: preceded by the octave number, followed by 'O' followed by
// the key number and the color code to display ('0' for no color). The response is a message in the form
// octaveNumber+'K'+key number+color code.
//////////////////////////////////////////////////////////////

#define HARD_SPEED 115200 
#define DBG_SPEED 9600
#define MIDI_SPEED 31250


//#include <Wire.h>

// See http://www.vlsi.fi/filedmin/datasheets/vs1053.pdf Pg 31
#define VS1053_BANK_DEFAULT 0x00
#define VS1053_BANK_DRUMS1 0x78
#define VS1053_BANK_DRUMS2 0x7F
#define VS1053_BANK_MELODY 0x79

// See http://www.vlsi.fi/fileadmin/datasheets/vs1053.pdf Pg 32 for more!
#define VS1053_GM1_OCARINA 80

#define MIDI_NOTE_ON  0x90
#define MIDI_NOTE_OFF 0x80
#define MIDI_CHAN_MSG 0xB0
#define MIDI_CHAN_BANK 0x00
#define MIDI_CHAN_VOLUME 0x07
#define MIDI_CHAN_PROGRAM 0xC0


//#define VS1053_MIDI Serial2 
// on a Mega/Leonardo you may have to change the pin to one that 
// software serial support uses OR use a hardware serial port!

 #include <SoftwareSerial.h>
  SoftwareSerial VS1053_MIDI(0, 2);

#define DebugSerial Serial
bool bFirst = true;

#define SlaveSerial Serial1    // 19/18

byte touche[ 12 ] = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0 };

byte stateReturnPath;
char msgFromSlave[ 100 ];
int valueInKeyboardMessage;
int changesInProcess;

byte touchStatus[ 8 ][ 12 ];
byte allInstr[ 8 ][ 12 ];
byte allNotes[ 8 ][ 12 ];
byte allChanl[ 8 ][ 12 ];
byte allBanks[ 8 ][ 12 ];
byte allColor[ 8 ][ 12 ];

byte octaves = 0;

#define VS_RESET  9 //Reset is active low

void midiSetInstrument(uint8_t chan, uint8_t inst) {
  if (chan > 15) return;
  inst --; // page 32 has instruments starting with 1 not 0 :(
  if (inst > 127) return;
  
  VS1053_MIDI.write(MIDI_CHAN_PROGRAM | chan);  
  VS1053_MIDI.write(inst);
}


void midiSetChannelVolume(uint8_t chan, uint8_t vol) {
  if (chan > 15) return;
  if (vol > 127) return;
  
  VS1053_MIDI.write(MIDI_CHAN_MSG | chan);
  VS1053_MIDI.write(MIDI_CHAN_VOLUME);
  VS1053_MIDI.write(vol);
}

void midiSetChannelBank(uint8_t chan, uint8_t bank) {
  if (chan > 15) return;
  if (bank > 127) return;
  
  VS1053_MIDI.write(MIDI_CHAN_MSG | chan);
  VS1053_MIDI.write((uint8_t)MIDI_CHAN_BANK);
  VS1053_MIDI.write(bank);
}

void midiNoteOn(uint8_t chan, uint8_t n, uint8_t vel) {
  if (chan > 15) return;
  if (n > 127) return;
  if (vel > 127) return;
  
  VS1053_MIDI.write(MIDI_NOTE_ON | chan);
  VS1053_MIDI.write(n);
  VS1053_MIDI.write(vel);
}

void midiNoteOff(uint8_t chan, uint8_t n, uint8_t vel) {
  if (chan > 15) return;
  if (n > 127) return;
  if (vel > 127) return;
  
  VS1053_MIDI.write(MIDI_NOTE_OFF | chan);
  VS1053_MIDI.write(n);
  VS1053_MIDI.write(vel);
}

//////////////////////////////////////////////////////////////
//Send a MIDI note-on message.  Like pressing a piano key
//channel ranges from 0-15
void noteOn( byte octave, byte touche, byte attack_velocity )
{
debug( "instr " );
DebugSerial.print( allInstr[ octave ][ touche ] );
debug( " - note - " );
DebugSerial.println( allNotes[ octave ][ touche ] );
midiSetInstrument( allChanl[ octave ][ touche ], allInstr[ octave ][ touche ] ); 
midiNoteOn( allChanl[ octave ][ touche ], allNotes[ octave ][ touche ], attack_velocity );
}

//////////////////////////////////////////////////////////////
//Send a MIDI note-off message.  Like releasing a piano key
void noteOff( byte octave, byte touche, byte release_velocity )
{
midiNoteOff( allChanl[ octave ][ touche ], allNotes[ octave ][ touche ], release_velocity );
}

void debug( char *txt )
{
DebugSerial.write( txt );
}

#define VS1053_RESET 9 // This is the pin that connects to the RESET pin on VS1053
void setupMidi()
{
debug("***** VS1053 init\n");
  
VS1053_MIDI.begin(31250); // MIDI uses a 'strange baud rate'
  
pinMode(VS1053_RESET, OUTPUT);
digitalWrite(VS1053_RESET, LOW);
delay(10);
digitalWrite(VS1053_RESET, HIGH);
delay(10);
  
midiSetChannelBank(0, VS1053_BANK_MELODY);
midiSetInstrument(0, VS1053_GM1_OCARINA);
midiSetChannelVolume(0, 127);
}

//////////////////////////////////////////////////////////////
void setup( )
{
delay( 800 );
debug( "***** setting up Serials\n" );

DebugSerial.begin( DBG_SPEED );
SlaveSerial.begin( HARD_SPEED );

while ( !DebugSerial );
while ( !SlaveSerial);

while ( SlaveSerial.available( ) ) SlaveSerial.read( );

debug( "***** MP3 Shield init - midi mode\n" );
setupMidi( );

/////////////////////////////
debug( "***** setup touches\n" );
for ( int i = 0; i < 8; i++ )
    for ( int j = 0; j < 12; j++ )
        touchStatus[ i ][ j ] = 0;

/////////////////////////////
debug( "***** setup keypad\n" );
changesInProcess = 0;

/////////////////////////////
// give a chance to slave to finish their setup too
delay( 1000 );

debug( "***** reset\n" );
debug( "-->R\n" );
SlaveSerial.write( "R\n" );
waitFeedback( 1000 );

first( );
debug( "***** fin setup\n" );

}

//////////////////////////////////////////////////////////////
void reset( )
{
while ( SlaveSerial.available( ) ) SlaveSerial.read( );
stateReturnPath = 0;

debug( "-->R\n" );
SlaveSerial.write( "R\n" );
}

//////////////////////////////////////////////////////////////
/*void resendAll( )
{
while ( SlaveSerial.available( ) ) SlaveSerial.read( );

SlaveSerial.write( "TTTTT\n" );
debug( "-->TTTTT\n" );
stateReturnPath = 0;
}*/

//////////////////////////////////////////////////////////////
// light a key - for midi playback
void ledOn( char octave, char touche )
{
char msg[ 10 ];
msg[ 0 ] = 'O';
msg[ 1 ] = 0x30 + octave;
msg[ 2 ] = 0x30 + touche;
msg[ 3 ] = 0x30 + allColor[ octave ][ touche ];
msg[ 4 ] = '\n';
msg[ 5 ] = 0;
debug( "-->" );
debug ( msg );
SlaveSerial.write( msg );
}

//////////////////////////////////////////////////////////////
// light a key - for midi playback
void ledOff( char octave, char touche )
{
char msg[ 10 ];
msg[ 0 ] = 'O';
msg[ 1 ] = 0x30 + octave;
msg[ 2 ] = 0x30 + touche;
msg[ 3 ] = 0x30; // color = 0 - leds off
msg[ 4 ] = '\n';
msg[ 5 ] = 0;
debug( "-->" );
debug ( msg );
SlaveSerial.write( msg );
}

//////////////////////////////////////////////////////////////
// global change of color for a key (at init)
void setLedColor( char octave, char touche, char color )
{
char msg[ 10 ];
msg[ 0 ] = 'P';
msg[ 1 ] = 0x30 + octave;
msg[ 2 ] = 0x30 + touche;
msg[ 3 ] = 0x30 + color;
msg[ 4 ] = '\n';
msg[ 5 ] = 0;
debug( "-->" );
debug ( msg );
SlaveSerial.write( msg );
waitFeedback( 1000 ) ;

}

//////////////////////////////////////////////////////////////

void messageFromSlave( char newIn )
{
if ( newIn == '$' ) newIn = '\n';

msgFromSlave[ stateReturnPath++ ] = newIn;
msgFromSlave[ stateReturnPath ] = 0;
if ( newIn == '\n' )
    { // fin de message
    debug( "<--" );
    debug( msgFromSlave );
    switch ( msgFromSlave[ 0 ] )
        {
        //  got from slaves
        case 'd': // nothing to do, already displayed
        case 'D': // nothing to do, already displayed
        //debug( msgReturnPath );
            break;
        case 'n': // Note status changed on slave
        {
        int octave = msgFromSlave[ 1 ] - 0x30;
        int touche = msgFromSlave[ 2 ] - 0x30;
        int status = msgFromSlave[ 3 ] - 0x30;
        if ( octave > 7 || touche > 12 || status > 2 )
            { // Wrong value
            reset( );
            debug( "reset fromserver, invalid data\n" );
            }
        else
            { // Manage touch status
            if ( status == 1 )
                { // pressed
                if ( changesInProcess != 0 )
                    {
                    byte octaveTmp = octave;
                    byte toucheTmp = touche;
                    DebugSerial.println( valueInKeyboardMessage );
                    DebugSerial.println( octave );
                    DebugSerial.println( touche );
                    switch ( changesInProcess )
                        {
                        case 1://change instrument
                            debug( "instr \n" );
                            while ( octaveTmp < 8 )
                                {
                                while ( toucheTmp < 12 )
                                    {
                                    allInstr[ octaveTmp ][ toucheTmp ] = valueInKeyboardMessage;
                                    toucheTmp++;
                                    }
                                toucheTmp = 0;
                                octaveTmp++;
                                }
                            changesInProcess = 0;
                            break;
                        case 2://change octave
                            debug( "transpo\n" );
                            while ( octaveTmp < 8 )
                                {
                                while ( toucheTmp < 12 )
                                    {
                                    allNotes[ octaveTmp ][ toucheTmp ] = (valueInKeyboardMessage + 1) * 12 + toucheTmp;
                                    allChanl[ octaveTmp ][ toucheTmp ]++;
                                    toucheTmp++;
                                    }
                                toucheTmp = 0;
                                octaveTmp++;
                                valueInKeyboardMessage++;
                                }
                            changesInProcess = 0;
                            break;
                        case 3://change color
                            changesInProcess = 0;
                            break;
                        case 4://change bank
                            changesInProcess = 0;
                            break;
                        }
                    }
                noteOn( octave, touche, 0x60 );
                touchStatus[ octave ][ touche ] = 1;
                }
            else if ( status == 2 )
                { // released
                noteOff( octave, touche, 0x60 );
                touchStatus[ octave ][ touche ] = 0;
                }
            }
        }
        break;
        case 'o':
        case 'p': //just feedback
            break;
        case 'r': // get maximum of octaves (from s)
        // use r too by security but should be needed
        //break;
        case 's':
            //octaves = max( octaves, msgReturnPath[ 1 ] - 0x30 );
            break;
        case 't':
        {
        int j = 1;
        int oct = msgFromSlave[ j++ ] - 0x30;
        for ( int i = 0; i < 12; i++ )
            {
            touchStatus[ oct ][ i ] = msgFromSlave[ j++ ] - 0x30;
            }
        }
        break;
        // configured on master
        case 'K': // use Keyboard to change instrument/octave/other
        {
        int idx = 1;
        switch ( msgFromSlave[ idx++ ] )
            {
            case 'A'://changement d'instrument
                changesInProcess = 1;
                valueInKeyboardMessage = atoi( &(msgFromSlave[ idx ]) );
                break;
            case 'B'://changement d'octave
                changesInProcess = 2;
                valueInKeyboardMessage = atoi( &(msgFromSlave[ idx ]) );
                break;
            case 'C'://changement de couleur de la touche
                changesInProcess = 3;
                valueInKeyboardMessage = atoi( &(msgFromSlave[ idx ]) );
                break;
            case 'D'://changement de banque midi
                changesInProcess = 4;
                valueInKeyboardMessage = atoi( &(msgFromSlave[ idx ]) );
                break;
            }

        }
        break;
        default:
            debug( "---------------------->> unknown code\n" );
            
            break;
        }
    stateReturnPath = 0;
    }
}

int waitFeedback( int valDelay )
{
stateReturnPath = 0;
msgFromSlave[ stateReturnPath ] = 0;

int i = 0;
bool bStop = false;
// wait for return
while ( !bStop )
    {
    delay( 1 );
    i++;
    if ( i > valDelay ) bStop = true;
    if ( !bStop )
      {
      while ( SlaveSerial.available( ) )
        {
        char c = SlaveSerial.read();
        messageFromSlave( c );
        if ( c == '\n' )
        return 1;
        }
      }
    }
    
debug( "***** wait failed\n" );
return 0;
}


//////////////////////////////////////////////////////////////
void first( )
{
debug( "***** Search Octaves\n" );
bool bEnd = false;
char msg[ 10 ];
octaves = 0;
msg[ 0 ] = 'S';
msg[ 1 ] = 0x30;//+octaves;
msg[ 2 ] = '\n';
msg[ 3 ] = 0;
while ( bEnd == false )
    {
    debug( "-->" );
    debug( msg );
    SlaveSerial.write( msg );
    if ( !waitFeedback( 1000 ) )
        {
        bEnd = true;
        }
    else
        {
        debug( "<--" );
        debug( msgFromSlave );
        if ( msgFromSlave[0] == 's' )
            {
            octaves++;
            msg[ 1 ] = octaves + 0x30;
            }
        }
    }

debug( "***** Found " );
DebugSerial.print( octaves );
DebugSerial.println( " octaves" );


debug( "Init Octaves\n" );
/*debug( msg );
SlaveSerial.write( msg );
*/
msg[ 0 ] = 'S';
msg[ 1 ] = 'd';
msg[ 2 ] = '\n';
msg[ 3 ] = 0;
debug( "***** Init Octaves\n" );
debug( "-->" );
debug( msg );
SlaveSerial.write( msg );

delay( 500 );

// here, I should know how many octaves are ready.
int note = 0;
if ( octaves > 7 ) octaves = 2;

switch ( octaves )
    {
    case 1:
        note = 60;
        break;
    case 2:
        note = 60;
        break;
    case 3:
        note = 48;
        break;
    case 4:
        note = 48;
        break;
    case 5:
        note = 36;
        break;
    case 6:
        note = 36;
        break;
    case 7:
        note = 24;
        break;
    case 8:
        note = 24;
        break;
    default:
        note = 48;
        break;
    }
int colordn = 1;
int colorup = 1;

// init Colors
debug( "***** Init colors\n" );
for ( int i = 0; i < octaves; i++ )
    {
    int colordn = 3;
    int colorup = 1;
    for ( int j = 0; j < 12; j++ )
        {
        if ( touche[ j ] == 1 )
            {
            allColor[ i ][ j ] = colorup;
            colorup++;
            if ( colorup > 5 ) colorup = 1;
            }
        else
            {
            allColor[ i ][ j ] = colordn;
            colordn--;
            if ( colordn < 1 ) colordn = 7;
            }
            //allColor[i][j] = 7;

        debug ("***** init color and instr\n");
        //setLedColor( i, j, allColor[ i ][ j ] ); // 'P'
//        delay( 100 );

        // set array
        allInstr[ i ][ j ] = 1;
        allChanl[ i ][ j ] = 0;
        allNotes[ i ][ j ] = note;
        //Note on channel 1 (0x90), some note value (note), middle velocity (0x45):

        noteOn( i, j, 127 ); 
        ledOn( i, j ); // 'O'
        delay( 100 );

        //Turn off the note with a given off/release velocity
        noteOff( i, j, 127 );
        ledOff( i, j ); // 'O'
        delay( 100 );
        note++;
        }
    }

    /*    for ( int i = 0; i < octaves; i++ ) {
            for ( int j = 0; j < 12; j++ ) {
            //Note on channel 1 (0x90), some note value (note), middle velocity (0x45):
            ledOn( i, j );
            delay( 200 );
            ledOff( i, j );
            }
            }*/

    /*    // init Notes
        debug.println( "Init keys" );
        for ( int i = 0; i < octaves; i++ ) {
        for ( int j = 0; j < 12; j++ ) {
        //Note on channel 1 (0x90), some note value (note), middle velocity (0x45):
        allNotes[i][j] = note;
        noteOn( i, j, 127 );
        ledOn( i, j );
        delay( 200 );

        //Turn off the note with a given off/release velocity
        noteOff( i, j, 127 );
        ledOff( i, j );
        note++;
        }
        }
        */
    //for ( int i = 0; i < octaves; i++ ) {
    //	for ( int j = 0; j < 12; j++ ) {
    //		ledOn( i, j );
    //	}
    //}

    //delay( 200 );
    /*
for (int z=0; z < 2; z++)
*/
/*
debug( "**** all notes off\n");
for ( int i = 0; i < octaves; i++ )
    {
    for ( int j = 0; j < 12; j++ )
        {
        ledOff( i, j );
        delay( 200 );
        }
    }
*/
debug( "***** Notes initialized\n" );

bFirst = false;
}

//////////////////////////////////////////////////////////////
void loop( )
{
//if ( bFirst == true ) first( );

if ( SlaveSerial.available( ) )
    {
    messageFromSlave( SlaveSerial.read( ) );
    }

if ( DebugSerial.available( ) )
    {
    messageFromSlave( DebugSerial.read( ) );
    }
}
