
/*  *********************************************
    DisplayMovingDataMultiSensorV3
    Realtime display and storage of CardioKit signals

    Author: Nathan Volman
    
    Created: March 1, 2020

    Development Environment Specifics:
    Eclipse IDE for Java Developers
	Version: 2018-12 (4.10.0)
	Build id: 20181214-0600
    Processing 3.4

    Hardware Specifications:
    CardioKit-PCB-R10
 *  *********************************************/
import java.io.BufferedWriter;
import java.io.FileWriter;
import java.io.IOException;
import java.time.format.DateTimeFormatter;  
import java.time.LocalDateTime;    
import processing.core.*;
import processing.net.*;
import processing.sound.*;


public class UsingProcessing extends PApplet {
	static final String folder = "E:/Library/Google Drive/MaximProject/OneLastPush/QuickCEP/GeneratedData/";
	static final int secondsToRun = 300;
	static final String	ipAddr = "192.168.1.133"; // IP address of the CardioKit

	Client myClient;
	static final int telPort = 23;
	SimpleTCP stcp;
	BufferedWriter br;
	StringBuilder  sb;
	
	// ECG Paper Emulation (25mm/sec)
	static final float DISPLAY_SIZE_DIAG_INCH     = 24; 													// Diagonal size of display
	static final float DISPLAY_WIDTH_IN_PIXELS 	  = 1920;
	static final float DISPLAY_HEIGHT_WIDTH_RATIO = (float) 9.0f/16.0f;
	static final float DISPLAY_WIDTH_IN_INCHES    = (float) (DISPLAY_SIZE_DIAG_INCH / Math.sqrt(1+Math.pow(DISPLAY_HEIGHT_WIDTH_RATIO, 2)));
	static final float DISPLAY_PPI 				  = DISPLAY_WIDTH_IN_PIXELS / DISPLAY_WIDTH_IN_INCHES;
	static final float DISPLAY_PPMM 			  = DISPLAY_PPI / 25.4f; 									// Display pixels per mm
	
	static final float DISPLAY_MV_PER_CODE 		  = (float) (3300.0f / Math.pow(2,16)); 					// ADC millivolts per code, 3.3V / 16bits
	static final float DISPLAY_MV_PER_MM 		  = 1.0f / 10.0f; 											// 10mm to 1mV
	static final float DISPLAY_VERTICAL_SCALING   = DISPLAY_MV_PER_CODE * DISPLAY_PPMM / DISPLAY_MV_PER_MM; // multiply a 3.3V 16-bit code by this to scale it to pixels like ecg paper
		
	// START: These variables must be identically set to those in the Teensy
	static final int 	  NUM_DATA_STREAMS 			  = 7;
	static final int 	  NUM_DATA_STREAMS_TO_DISPLAY = 6;
	static final int 	  CORE_SAMPLE_FREQ 			  = 400;
	static final int 	  Fs 						  = NUM_DATA_STREAMS * CORE_SAMPLE_FREQ; 				// Sample rate in samples/second for single channel to display
	static final String[] CHANNEL_NAMES 		 	  = { "ECG0", "ECG1", "ECG2", "ECG3", "ECG4", "PCG", "ECG1", "ECG2", "ECG3", "ECG4", "ECG0", "ECG1", "ECG2", "ECG3", "ECG4", "AUSC", "ACCEL" };
	static final int 	  PCG_CHANNEL			      = 5; // 0 indexed so if 3 channel, one is ecg and 2 is pcg set this to 1
	static final int 	  ACCEL_CHANNEL			      = 6; // 0 indexed so if 3 channel, one is ecg and 2 is pcg set this to 2
	static final int 	  CHANNEL_BUFFER_LENGTH 	  = 359/(NUM_DATA_STREAMS);
	static final boolean  PCG_PRESENT                 = true;
	static final boolean  ACCEL_PRESENT               = true;
	// END:   These variables must be identically set to those in the Teensy
	
	// Split buffers for each signal
	int[][] channelBuffer     = new int[NUM_DATA_STREAMS][(secondsToRun+5) * CORE_SAMPLE_FREQ + 1000];
	int[]   channelBufferTail = new int[NUM_DATA_STREAMS];
	int 	stcpParseIndex 	  = 0;
	//buffer valid indices and such, split the buffers and display them as theyre ready
	
	// Display Variables
	static final int BACKGROUND_COLOR 	  		= 0;
	static final int STROKE_COLOR 		  		= 255;
	static final int LINES_TO_BLANK_AHEAD 		= 10;
	static final int traceHeight 		  		= 200;
	static final int traceWidth  		  		= 900;
	static final int EXTRA_SAMPLES_PER_CHANNEL 	= 250; // keep about 250 samples worth of time buffer per channel for fluid streaming
	
	// Display Variables for Display Version 2
	static final int[] DisplayChannelOffsets = { 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100, 1200, 1300, 1400, 1500};
	PGraphics pgWaterfall = new PGraphics();
	static final int[] CHANNEL_SCALING = {2,2,2,2,2,2,2,1,1,1,1,1,1,1,1,1};
	int[][] DisplayBuffer = new int[NUM_DATA_STREAMS][width];
	static final int TIME_COMPRESSION_FACTOR = 1;
	int DisplayBufferIndex = 0;
	static final int WIDTH_SECOND_PIXELS = CORE_SAMPLE_FREQ / TIME_COMPRESSION_FACTOR;
	static final int ECG_TOTAL_GAIN = 780;
	static final int ADC_FULL_SCALE_MV = 3300;
	static final int ONE_MV_ECG_INPUT_PIXELS = traceHeight*CHANNEL_SCALING[0]*ECG_TOTAL_GAIN/ADC_FULL_SCALE_MV;
	PFont TEXT_FONT;
	
	long  tStart, tLast, tNow;
	float frameResidual 			 = 0;
	int   totalSamplesDisplayedPerCh = 0;
	int   samplesToDisplayPerCh      = 0;
	int[] yPrevious 			     = new int[NUM_DATA_STREAMS];
	
	float 			   AverageDataRate;
	static final int   FrameRate    = 100;
	static final int   width 		= 1900;
	static final int   height 		= 1000;
	static final float buttonWidth 	= 100;
	static final float buttonHeight = 25;
	static final float buttonX 		= width - buttonWidth - 1;
	static final float buttonY 		= 0;
	
	// Audio playback of PCG Variables, Implementing Double Buffering in one physical buffer
	AudioSample audio;
	static final int AUDIO_PLAYBACK_DELAY_MILLIS = 250; // buffer X milliseconds of audio before starting playback
	static final int AUDIO_BUF_HALF_SIZE = (AUDIO_PLAYBACK_DELAY_MILLIS*CORE_SAMPLE_FREQ)/1000;
	static final int AUDIO_BUF_SIZE      = 2*AUDIO_BUF_HALF_SIZE;
	float[] audioDoubleBuf 		 = new float[AUDIO_BUF_SIZE]; // playback buffer stores 1 second of samples
	float[] audioDoubleBufShadow = new float[AUDIO_BUF_SIZE]; // buffer which is periodically copied into audioDoubleBuf
	boolean startedPlayback = false;
	int audioChannelHead = 0;
	float learnedAudioScalingFactor = 1;
	static final float SIGNAL_DETECTION_CUTOFF = 0.2f;
	
	static final int TotalSamplesToCollect = Fs*secondsToRun;
	String[] samplesString = new String[TotalSamplesToCollect];
	
	PImage cardiokit;
	PGraphics pgCardioKit;
	static final int CardioKitPosX = 100;
	static final int CardioKitPosY = 100;
	static final int CardioKitHeight = 200;
	static final int CardioKitWidth  = 200;
	int angle = 0;
	
	public static void main(String[] args) {
		PApplet.main("UsingProcessing");
	}

	public void settings() {
		size(width, height);
		pixelDensity(displayDensity(1));
	}

	public void setup() {
		InitializeHighPassAcculumator();
		cardiokit = loadImage ("CardioKitBlackBackground200x200.png");
		pgCardioKit = createGraphics(CardioKitWidth,CardioKitHeight);
		
		pgWaterfall = createGraphics(width,height);
		pgWaterfall.stroke(STROKE_COLOR);
		
		frameRate(FrameRate);
		stroke(STROKE_COLOR);
		TEXT_FONT = createFont("Georgia", 64);
		
		// Connect to the server's IP address and port
		myClient = new Client(this, ipAddr, telPort);
		myClient.clear();

		// Setup SimpleTCP Protocol
		stcp = new SimpleTCP(this);
				
		// Send start ACK to begin data stream from Host
		SimpleTcpAcket startAck = new SimpleTcpAcket();
		startAck.SendStartAcket(myClient);
		this.delay(250);
		// keep resending startAck until data is received
		while(myClient.available() <=0) {
			this.delay(250);
			startAck = new SimpleTcpAcket();
			startAck.SendStartAcket(myClient);
		}
		tStart = System.nanoTime();
		System.out.println("tStart:" + tStart);
		// buffer some time to handle dropped packets
		tLast  = tStart + ((long) (1000000000f * EXTRA_SAMPLES_PER_CHANNEL/CORE_SAMPLE_FREQ));
	}
	
	public void DrawCardioKitRotation() {
		pgCardioKit.beginDraw();
		pgCardioKit.background(BACKGROUND_COLOR);
		pgCardioKit.translate(pgCardioKit.width/2, pgCardioKit.height/2);
		pgCardioKit.rotate(radians(angle));
		pgCardioKit.translate(-pgCardioKit.width/2, -pgCardioKit.height/2);
	    pgCardioKit.image(cardiokit, 0, 0);
	    pgCardioKit.endDraw();
	    pgCardioKit.beginDraw();
	    pgCardioKit.textAlign(CENTER, CENTER);
	    pgCardioKit.textSize(32);
	    pgCardioKit.fill(0, 102, 153, 204);
	    pgCardioKit.text(angle, pgCardioKit.width/2, pgCardioKit.height/2);
	    pgCardioKit.endDraw();
	}
	
	// gives the channel number corresponding to a 16-bit index in a channel packed like STCP
	public int WhichChannel(int index) {
		return Math.floorDiv(index, CHANNEL_BUFFER_LENGTH) % NUM_DATA_STREAMS;
	}
	
	// gives the entry number corresponding to a 16-bit index in a channel packed like STCP
	public int WhichEntry(int index) {
		return Math.floorDiv(index, CHANNEL_BUFFER_LENGTH * NUM_DATA_STREAMS) * CHANNEL_BUFFER_LENGTH + (index % CHANNEL_BUFFER_LENGTH);
	}
	
	public void HandleShutdownAndSave() {
		stcp.RequestReset(myClient);
		
		int index = 0;
		int totalSamplesPerChannel = CORE_SAMPLE_FREQ*secondsToRun;
		for(int ch = 0; ch < NUM_DATA_STREAMS; ch++) {
			for(int entry = 0; entry < totalSamplesPerChannel; entry++) {
				index = ch*totalSamplesPerChannel + entry;
				samplesString[index] = String.valueOf(channelBuffer[ch][entry]);
			}
		}
		
		try {
			DateTimeFormatter dtf = DateTimeFormatter.ofPattern("yyyy_MM_dd_HH_mm_ss");  
		    LocalDateTime now = LocalDateTime.now();  
		    System.out.println(dtf.format(now));  
		    StringBuilder filename = new StringBuilder();
		    filename.append(folder);
		    filename.append("ck");
		    filename.append(dtf.format(now));
		    filename.append(".csv");
		    br = new BufferedWriter(new FileWriter(filename.toString()));
		} catch (IOException e1) {
			e1.printStackTrace();
		}
		sb = new StringBuilder();
		
		// First element is Per Channel Sample Rate
		sb.append(CORE_SAMPLE_FREQ);
		sb.append(",");
		// Second element is Number of channels
		sb.append(NUM_DATA_STREAMS);
		sb.append(",");
		// Third element is secondsToRun
		sb.append(secondsToRun);
		sb.append(",");

		// Append strings from array
		for (String element : samplesString) {
		 sb.append(element);
		 sb.append(",");
		}
		sb.append("-1");
		
		try {
			br.write(sb.toString());
		} catch (IOException e) {
			e.printStackTrace();
		}
		try {
			br.close();
		} catch (IOException e) {
			e.printStackTrace();
		}
		System.out.println("Done Saving");
		System.exit(0);
	}
	
	public void SeparateStreamIntoChannels() {
		int validUntil = Math.floorDiv(stcp.dataValidUntil, 2);
		
		// validUntil now stores the 16-bit index into stcp.rxData of the last contiguously valid 16-bit word
		// while there are 16-bit words left to parse
		while(stcpParseIndex < validUntil) {
			int ch = WhichChannel(stcpParseIndex);
			int entry = WhichEntry(stcpParseIndex);
			int val  = ((stcp.rxData[stcpParseIndex*2 + 1]) & 0x000000FF) << 8;
			val     |= ((stcp.rxData[stcpParseIndex*2    ]) & 0x000000FF);	
			channelBuffer[ch][entry] = val;
			if(PCG_PRESENT && (ch == PCG_CHANNEL)) { 
				audioDoubleBufShadow[entry % AUDIO_BUF_SIZE] = (3.0f*((float)val)/65536f)-1.0f;
			}
			else if(ACCEL_PRESENT && (ch == ACCEL_CHANNEL)) {
				// store the most recent device tilt angle for display
				angle = channelBuffer[ch][entry];
				System.out.println(angle);
			}
			channelBufferTail[ch]++;
			stcpParseIndex++;
		}
		if(validUntil >= TotalSamplesToCollect) {
			HandleShutdownAndSave();
		}
	}
	
	public void MovingAverage() {
		// Implement a single moving average filter
		int WINDOW_SIZE = 5;
		int acc;
		for(int ch = 0; ch < NUM_DATA_STREAMS_TO_DISPLAY; ch++) {
			for(int x = 0; x < width; x++) {
				acc = 0;
				int startPos = (x-WINDOW_SIZE+width)%width;
				for(int i = startPos; i < x; i++) {
					i%=width;
					acc += DisplayBuffer[ch][i];
				}
				DisplayBuffer[ch][x] = acc/WINDOW_SIZE;
			}
		}
	}
	
	static final int HIGHPASS_MEM_LEN  = 3;
	double[][] HighPassAccumulator = new double[NUM_DATA_STREAMS_TO_DISPLAY][HIGHPASS_MEM_LEN];
	double[][] LowPassAccumulator = new double[NUM_DATA_STREAMS_TO_DISPLAY][HIGHPASS_MEM_LEN];
	
	public void InitializeHighPassAcculumator() {
		for(int i = 0; i < NUM_DATA_STREAMS_TO_DISPLAY; i++) {
			for(int j = 0; j < HIGHPASS_MEM_LEN; j++) {
				HighPassAccumulator[i][j] = 0;
				LowPassAccumulator[i][j] = 0;
			}
		}
	}
	
	// Currently this will filter at 1/200th of the sample rate. 2nd order Bessel Highpass at 1Hz for Fs = 200samples/sec
	//schwietering.com/jayduino/filtuino/index.php?characteristic=be&passmode=hp&order=2&usesr=usesr&sr=200&frequencyLow=1&noteLow=&noteHigh=&mz=mz&pw=pw&calctype=float&run=Send
	public int HighPassRealtime(int newsample, int channel) {
		double Ax,A1,A0;
		if(CORE_SAMPLE_FREQ <= 200) {
			Ax = 9.789112261464527620e-1;
			A0 = -0.95812105714554918201;
			A1 = 1.95752384744026186603;
		} else if (CORE_SAMPLE_FREQ <= 250) {
			Ax = 9.830816006975070520e-1;
			A0 = -0.96635511129264772823;
			A1 = 1.96597129149738081288;
		} else if (CORE_SAMPLE_FREQ <= 500) {
			Ax = 9.914929533299428055e-1;
			A0 = -0.98303429081417403879;
			A1 = 1.98293752250559740524;
		} else if (CORE_SAMPLE_FREQ <= 750) {
			Ax = 9.943179096871302969e-1;
			A0 = -0.98865738439678685356;
			A1 = 1.98861425435173488907;
		} else if (CORE_SAMPLE_FREQ <= 1000) {
			Ax = 9.957343968450440563e-1;
			A0 = -0.99148094123373253783;
			A1 = 1.99145664614644379853;
		} else if (CORE_SAMPLE_FREQ <= 1250) {
			Ax = 9.965855770779779021e-1;
			A0 = -0.99317893521124456235;
			A1 = 1.99316337310066660216;
		} else if (CORE_SAMPLE_FREQ <= 1500) {
			Ax = 9.971535683844882092e-1;
			A0 = -0.99431254335229934949;
			A1 = 1.99430173018565337628;
		} else if (CORE_SAMPLE_FREQ <= 1750) {
			Ax = 9.975595402529929823e-1;
			A0 = -0.99512305430376690740;
			A1 = 1.99511510670820513269;
		} else { // if (CORE_SAMPLE_FREQ <= 2000) {
			Ax = 9.978641635582454761e-1;
			A0 = -0.99573137048294946272;
			A1 = 1.99572528375003210854;
		}
		
		HighPassAccumulator[channel][0] = HighPassAccumulator[channel][1];
		HighPassAccumulator[channel][1] = HighPassAccumulator[channel][2];
		HighPassAccumulator[channel][2] = (Ax * newsample) + (A0 * HighPassAccumulator[channel][0]) + (A1 * HighPassAccumulator[channel][1]);
		int toReturn = (int)(HighPassAccumulator[channel][0] + HighPassAccumulator[channel][2] - (2*HighPassAccumulator[channel][1]));
		return toReturn;
	}
	
	// Currently this will filter at 2/5th of the sample rate. 2nd order Bessel Lowpass at 80Hz for Fs = 200samples/sec
	//schwietering.com/jayduino/filtuino/index.php?characteristic=be&passmode=lp&order=2&usesr=usesr&sr=200&frequencyLow=80&noteLow=&noteHigh=&mz=mz&pw=pw&calctype=float&run=Send
	public int LowPassRealtime(int newsample, int channel) {
		double Ax,A1,A0;
		if(CORE_SAMPLE_FREQ <= 200) {
			Ax = 6.632725550263798286e-1;
			A0 = -0.41309897388241467731;
			A1 = -1.23999124622310508137;
		} else if (CORE_SAMPLE_FREQ <= 250) {
			Ax = 4.732518040613218346e-1;
			A0 = -0.18209595672758702167 ;
			A1 = -0.71091125951770051117;
		} else if (CORE_SAMPLE_FREQ <= 500) {
			Ax = 1.811019649995656844e-1;
			A0 = -0.10287893926164592973;
			A1 = 0.37847107926338319217;
		} else if (CORE_SAMPLE_FREQ <= 750) {
			Ax = 9.993473079761497346e-2;
			A0 = -0.21848281633851865391;
			A1 = 0.81874389314805873230;
		} else if (CORE_SAMPLE_FREQ <= 1000) {
			Ax = 6.378257264840968277e-2;
			A0 = -0.32348469761337356188;
			A1 = 1.06835440701973483080;
		} else if (CORE_SAMPLE_FREQ <= 1250) {
			Ax = 4.432912946233446422e-2;
			A0 = -0.40769135338404427493;
			A1 = 1.23037483553470639031;
		} else if (CORE_SAMPLE_FREQ <= 1500) {
			Ax = 3.262159235579251138e-2;
			A0 = -0.47475435073367833194;
			A1 = 1.34426798131050828644;
		} else if (CORE_SAMPLE_FREQ <= 1750) {
			Ax = 2.501913368979469254e-2;
			A0 = -0.52884157090189343187 ;
			A1 = 1.42876503614271466169;
		} else { // if (CORE_SAMPLE_FREQ <= 2000) {
			Ax = 1.980014076208944096e-2;
			A0 = -0.57316431459546379745;
			A1 = 1.49396375154710603361;
		}
		
		LowPassAccumulator[channel][0] = LowPassAccumulator[channel][1];
		LowPassAccumulator[channel][1] = LowPassAccumulator[channel][2];
		LowPassAccumulator[channel][2] = (Ax * newsample) + (A0 * LowPassAccumulator[channel][0]) + (A1 * LowPassAccumulator[channel][1]);
		int toReturn = (int)(LowPassAccumulator[channel][0] + LowPassAccumulator[channel][2] + (2*LowPassAccumulator[channel][1]));
		return toReturn;
	}
	
	public void DisplayNewSamplesV2() {
		if(samplesToDisplayPerCh == 0) {return;}
		
		// Make sure we have a multiple of TIME_COMPRESSION_FACTOR samples to display since we will be averaging
		int residualSamples = samplesToDisplayPerCh % TIME_COMPRESSION_FACTOR;
		samplesToDisplayPerCh -= residualSamples;
		frameResidual         += residualSamples;
		
		int acc;
		// update the display buffer of each channel with appropriate per-channel scaling and temporal compression
		for(int x = totalSamplesDisplayedPerCh; x < (totalSamplesDisplayedPerCh+samplesToDisplayPerCh); x += TIME_COMPRESSION_FACTOR) {
			for(int ch = 0; ch < NUM_DATA_STREAMS_TO_DISPLAY; ch++) {
				acc = 0;
				for(int i = 0; i < TIME_COMPRESSION_FACTOR; i++) {
					acc += channelBuffer[ch][x+i];
				}
				// Scale the sample to account for TIME_COMPRESSION_FACTOR averaging to the traceHeight desired
				acc /= TIME_COMPRESSION_FACTOR;
				acc = (int) map(acc, 0, 65535, traceHeight, 0); // this flips the waveforms to account for y=0 being top of display
				acc *= CHANNEL_SCALING[ch];
				
				acc = HighPassRealtime(acc, ch); // The highpass corner is sample-rate dependent (for now)
				acc = LowPassRealtime(acc, ch); // The lowpass corner is sample-rate dependent (for now)
				
				// Offset this channel's trace by the desired amount for pretty display
				acc += DisplayChannelOffsets[ch];
				DisplayBuffer[ch][DisplayBufferIndex] = acc;
			}
			DisplayBufferIndex++;
			DisplayBufferIndex%=width;
		}
		
		// Figure out where the blanking interval is right now
		int blankingStartIndex = ((totalSamplesDisplayedPerCh+samplesToDisplayPerCh)/TIME_COMPRESSION_FACTOR) % width;
		int blankingStopIndex = (blankingStartIndex + LINES_TO_BLANK_AHEAD) % width;
		
		// If the accelerometer is streaming, build a scene with CardioKit rotation
		if(ACCEL_PRESENT) {DrawCardioKitRotation();}
		pgWaterfall.beginDraw();
		pgWaterfall.background(BACKGROUND_COLOR);
		
		// Display time marker lines in an easily readable color
		pgWaterfall.stroke(128);
		
		for(int x = (WIDTH_SECOND_PIXELS%width); x < width; x+=WIDTH_SECOND_PIXELS) {
			int STRIDE = 10;
			int LINEHEIGHT = 5;
			for(int y = STRIDE; y < height; y+=STRIDE) {
				pgWaterfall.line(x, y, x, y-LINEHEIGHT);
			}
		}
		
		// Display 1mV marker lines in an easily readable color
		int yLower = DisplayChannelOffsets[0];
		int yUpper = DisplayChannelOffsets[NUM_DATA_STREAMS_TO_DISPLAY];// + traceHeight + 15;
		for(int y = yLower; y < yUpper; y += ONE_MV_ECG_INPUT_PIXELS/4) {
			int XSTRIDE = 10;
			int XLINEWIDTH = 5;
			for(int x = XSTRIDE; x < width; x+=XSTRIDE) {
				pgWaterfall.line(x-XLINEWIDTH, y, x, y);
			}
		}
		
		// If the accelerometer is streaming, add live CardioKit rotation to the display window
		if(ACCEL_PRESENT) { pgWaterfall.image(pgCardioKit, width - pgCardioKit.width, height-pgCardioKit.height);}
		
		pgWaterfall.stroke(STROKE_COLOR);
		// draw lines for the portion of the buffer up to the blanking interval
		for(int x = 0; x < (blankingStartIndex-1); x++) {
			for(int ch = 0; ch < NUM_DATA_STREAMS_TO_DISPLAY; ch++) {
				pgWaterfall.line(x, DisplayBuffer[ch][x], (x+1) % width, DisplayBuffer[ch][(x+1) % width]);
			}
		}
		// draw lines for the portion of the buffer after the blanking interval
		for(int x = blankingStopIndex; x < (width-1); x++) {
			for(int ch = 0; ch < NUM_DATA_STREAMS_TO_DISPLAY; ch++) {
				pgWaterfall.line(x, DisplayBuffer[ch][x], (x+1) % width, DisplayBuffer[ch][(x+1) % width]);
			}
		}
		
		// Display text
		pgWaterfall.textFont(TEXT_FONT);
		pgWaterfall.textSize(32);
		pgWaterfall.text("1sec/div",10,40);
		pgWaterfall.text("250uV/div",10,80);
		pgWaterfall.textSize(16);
		for(int ch = 0; ch < NUM_DATA_STREAMS_TO_DISPLAY; ch++) {
			int textY = DisplayBuffer[ch][0]+15;
			pgWaterfall.fill(BACKGROUND_COLOR);
			pgWaterfall.rectMode(CORNER);
			pgWaterfall.rect(8,textY-18,textWidth(CHANNEL_NAMES[ch]) + 15,25);
			pgWaterfall.fill(STROKE_COLOR);
			pgWaterfall.text(CHANNEL_NAMES[ch], 10, textY);
		}
		
		// write the buffer to the display
		pgWaterfall.endDraw();
		image(pgWaterfall, 0, 0, width, height);
		// update the external display counter
		totalSamplesDisplayedPerCh += samplesToDisplayPerCh;
	}

	public void HandleStartingAudioStream() {
		// Fill the audioDoubleBuf
		int audioDoubleBufStartIndex = audioChannelHead % AUDIO_BUF_SIZE;
		int samplesToCopy =  channelBufferTail[PCG_CHANNEL] - audioChannelHead;
		
		if(samplesToCopy >= AUDIO_BUF_SIZE) { 
			System.out.println("ERROR: AUDIO BUFFERING OVERFLOW" + samplesToCopy);
			samplesToCopy = AUDIO_BUF_SIZE;
		} else if (samplesToCopy >= AUDIO_BUF_HALF_SIZE) { 
			System.out.println("WARNING: AUDIO BUFFERING OVERFLOW" + samplesToCopy); 
		}
		
		if(samplesToCopy > 0) {
			if(!startedPlayback) {
				// if we haven't started playback, just copy all of the shadow buffer in the audioDoubleBuf
				System.arraycopy( audioDoubleBufShadow, 0, audioDoubleBuf, 0, AUDIO_BUF_SIZE );
			} else {
				if(samplesToCopy + audioDoubleBufStartIndex > AUDIO_BUF_SIZE) {
					// the buffer is wrapping, break up the write into two
					int firstWriteLen = AUDIO_BUF_SIZE - audioDoubleBufStartIndex;
					//System.out.println("firstWriteLen" + firstWriteLen);
					audio.write(audioDoubleBufStartIndex, audioDoubleBufShadow, audioDoubleBufStartIndex, firstWriteLen);
					audio.write(0, 						  audioDoubleBufShadow, 0, 						  samplesToCopy-firstWriteLen);
				} else {
					audio.write(audioDoubleBufStartIndex, audioDoubleBufShadow, audioDoubleBufStartIndex, samplesToCopy);
				}
			}
			audioChannelHead += samplesToCopy;
		}
		
		if(!startedPlayback) {
			long timeToWait = AUDIO_PLAYBACK_DELAY_MILLIS * 1000000L;
			long timeSinceStart = System.nanoTime()-tStart;
			if(timeSinceStart >= timeToWait) {
				// we've waiting the necessary amount of time to buffer audio samples, start playing the samples on loop
				startedPlayback = true;
				audio = new AudioSample(this, audioDoubleBuf, false, CORE_SAMPLE_FREQ);
				audio.rate(1);
				audio.amp(1.0f);
				audio.loop();
				System.out.println("Start Audio Playback");
			}
		}
	}
	
	public void HandleAudioStreamShutdown() {}
	
	public void ScaleAudio() {
		// scale the audio to +1/-1
		float max = 0;
		for(int i = 0 ; i < AUDIO_BUF_SIZE; i++) {
			if(audioDoubleBufShadow[i] > max) { max = audioDoubleBufShadow[i]; }
		}
		learnedAudioScalingFactor = 2/max;
		System.out.println("MAX: " + max);
		System.out.println("SCL: " + learnedAudioScalingFactor);
		if(max < SIGNAL_DETECTION_CUTOFF) { learnedAudioScalingFactor = 0; }
		for(int i = 0 ; i < AUDIO_BUF_SIZE; i++) {
			audioDoubleBufShadow[i] = (audioDoubleBufShadow[i]*learnedAudioScalingFactor) - 1;
		}
	}
	
	// this is called every 1/FrameRate seconds
	public void draw() {
		// Handle click-to-exit
		if(mousePressed){
			if(mouseX>buttonX && mouseX <buttonX+buttonWidth && mouseY>buttonY && mouseY <buttonY+buttonHeight){
				stcp.RequestReset(myClient);
				if(PCG_PRESENT) { HandleAudioStreamShutdown(); }
				System.exit(0);
			}
		}
		
		// Parse the incoming data
		stcp.ParseStream(myClient);
		stcp.RequestResend(myClient);
		SeparateStreamIntoChannels();
		
		if(PCG_PRESENT) { HandleStartingAudioStream(); }		
		
		// Calculate how many samples per channel to display this frame
		tNow = System.nanoTime();
		float SecondsSinceLastFrame = (float)(tNow - tLast) / 1000000000L;
		if(SecondsSinceLastFrame > 0) {
			float fractionalSamplesToDisplay = CORE_SAMPLE_FREQ*SecondsSinceLastFrame + frameResidual;
			
			// save residual samples to a value and add it in every time this handles timing jitter
			frameResidual = fractionalSamplesToDisplay - ((float)Math.floor(fractionalSamplesToDisplay)); 
			samplesToDisplayPerCh = (int) fractionalSamplesToDisplay;
		} else {
			samplesToDisplayPerCh = 0;
		}
		tLast = tNow;
		
		// handle case where not enough samples have arrived yet to display
		// figure out last byte of packet in which the data we are trying to display is located
		int packetNumberOfSample = 1 + ((samplesToDisplayPerCh + totalSamplesDisplayedPerCh)/CHANNEL_BUFFER_LENGTH);
		int lastByteNeeded = (2*NUM_DATA_STREAMS*CHANNEL_BUFFER_LENGTH*packetNumberOfSample) - 1;
		if( stcp.dataValidUntil <= lastByteNeeded) {
			// there are not enough samples present yet to display so don't display any and save them for next frame
			frameResidual += samplesToDisplayPerCh;
			samplesToDisplayPerCh = 0;
		}
		
		// we are guaranteed that any samples we try to display are valid, display them
		DisplayNewSamplesV2();
	}
}

