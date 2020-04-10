
/*  *********************************************
    SimpleTCP.java
    Code to receive data from CardioKit with SimpleTCP Packet Structure

    Nathan Volman
    Created: Jan 25, 2019

    Development Environment Specifics:
    Eclipse IDE for Java Developers
	Version: 2018-12 (4.10.0)
	Build id: 20181214-0600
    Processing 3.4

    Hardware Specifications:
    CardioKit-PCB-R10
 *  *********************************************/
import processing.net.*;
import processing.core.*;

class SimpleTCP {
	PApplet				parent;
	SimpleTcpPacket		s;
	byte[]				rxData;
	boolean[]			dataValid;				// 1 if that byte has been received successfully, zero else
	int					dataValidUntil;			// Index of the first element in rxData that is not valid
	int					latestValidByte;		// Index of the highest indexed byte received so far
	byte[]				buffer;					// for taking in new samples from the stream
	int					bufferHead;				// location of the first valid byte
	int					bufferTail;				// location of the first free byte
	static final int	bufferSize	= 1000000;
	static final int	rxDataSize	= 500000000;
	static final int	resendRequestBufLen = 1000;
	int[]				resendRequestStartBytes;
	int[]				resendRequestEndBytes;
	long[]				resendRequestTimestamps;
	int					resendRequestTail;
	static final long	resendRequestMinimumGap = 350*1000*1000; // Minimum amount of time in nanoseconds between identical resend nacks
																 // 350 seems to be sweet spot

	SimpleTCP(PApplet p) {
		parent			= p;
		s				= new SimpleTcpPacket();
		rxData			= new byte[rxDataSize];		// 100 MBytes
		dataValid		= new boolean[rxDataSize];
		buffer			= new byte[bufferSize];
		dataValidUntil	= 0;
		latestValidByte	= 0;
		bufferHead		= 0;
		bufferTail		= 0;
		resendRequestStartBytes = new int[resendRequestBufLen];
		resendRequestEndBytes = new int[resendRequestBufLen];
		resendRequestTimestamps = new long[resendRequestBufLen];
		resendRequestTail = 0;
	}

	long PrintStatus() {
		System.out.println();
		System.out.println("dataValidUntil: " + this.dataValidUntil);
		System.out.println("bufferHead:     " + this.bufferHead);
		System.out.println("bufferTail:     " + this.bufferTail);
		System.out.println();
		System.out.println("Valid Intervals...");
		long	totalBytesRx	= 0;
		int		start			= 0;
		boolean	midFlag			= false;
		for (int i = 0; i < rxDataSize; i++) {
			if (this.dataValid[i]) {
				if (!midFlag) {
					start = i;
					System.out.print(i + ":");
					midFlag = true;
				}
			} else if (midFlag) {
				totalBytesRx += (i - start);
				System.out.println(i);
				midFlag = false;
			}
		}

		System.out.println("Total Bytes Rx: " + totalBytesRx);
		return totalBytesRx;
	}

	// Take a known valid packet stored in s and incorporate its elements into
	// rxData and dataValid
	void IncorporatePacket() {
		if (this.dataValid[this.s.sequenceNumber]) {
			System.out.println("ERROR: Data Already Encountered");
			System.out.println("ReReceive:" + this.s.sequenceNumber);
				System.out.println(System.nanoTime()); //temp
				System.exit(0); //temp
		}
		
		// Data is new and valid, incorporate it
		for (int i = 0; i < this.s.byteLength; i++) {
			this.rxData[i + this.s.sequenceNumber]		= this.s.data[i];
			this.dataValid[i + this.s.sequenceNumber]	= true;
		}

		// Update the index of the last valid byte
		if (this.latestValidByte < (this.s.byteLength + this.s.sequenceNumber - 1)) {
			this.latestValidByte = this.s.byteLength + this.s.sequenceNumber - 1;
		}

		// Update the index of the first invalid byte
		if (this.dataValidUntil >= this.s.sequenceNumber && this.dataValidUntil <= (this.s.sequenceNumber + this.s.byteLength)) {
			// This new packet held the oldest missing bytes
			int idx = this.s.sequenceNumber + this.s.byteLength;
			while (this.dataValid[idx]) {
				idx++;
			}
			this.dataValidUntil = idx;
		}
	}

	void ParseStream(Client c) {
		int bytesAvailable = c.available();

		// Take in as many bytes as possible
		while ((bytesAvailable > 0) && (((this.bufferTail + 1)
				% bufferSize) != this.bufferHead)) { /* always leave one byte empty to know empty array from full */
			// Data available
			this.buffer[this.bufferTail]	= (byte) c.read();
			this.bufferTail					= (this.bufferTail + 1) % bufferSize;
			bytesAvailable--;
		}

		// Skip over bytes that aren't packetSignifer (0xAA) and note how many skipped
		int bytesSkipped = 0;
		// while (bufferHead doesn't point at a packetSignifier)

		int bytesInBuffer = (bufferSize - this.bufferHead + this.bufferTail) % bufferSize;

		// while there are enough bytes in the buffer to be a packet
		while (bytesInBuffer > 12) {
			if (((byte) this.buffer[bufferHead]) != ((byte) 0xAA)) {
				// This is not the beginning of a valid packet
				if (((this.bufferHead + 1) % bufferSize) != this.bufferTail) {
					// make sure you don't overrun the bufferTail
					this.bufferHead = (this.bufferHead + 1) % bufferSize; // pop the first element out of the buffer
					bytesInBuffer--;
				} else {
					// Should never reach this
					System.out.println("ERROR: Buffer Was About to OVERRUN");
				}
			} else {
				// The first packet Signifier was present, check for the second
				if (((byte) this.buffer[((this.bufferHead + 4) % bufferSize)]) != ((byte) 0x55)) {
					// the second packet signifier was not present or was corrupted
					if (((this.bufferHead + 1) % bufferSize) != this.bufferTail) {
						// make sure you don't overrun the bufferTail
						this.bufferHead = (this.bufferHead + 1) % bufferSize; // pop the first element out of the buffer
						bytesInBuffer--;
					} else {
						System.out.println("ERROR: Buffer Was About to OVERRUN");
					}
				} else {
					// the second packet signifier was found, this could be a packet. Break out of the loop
					bytesInBuffer = 0;
					break;
				}
			}
		}

		// check if both signifiers are present, if not then we have just run out of
		// buffer and should return later
		bytesInBuffer = (bufferSize - this.bufferHead + this.bufferTail) % bufferSize;
		if ((bytesInBuffer <= 12) || ((byte) this.buffer[bufferHead]) != ((byte) 0xAA)
				|| ((byte) this.buffer[((this.bufferHead + 4) % bufferSize)]) != ((byte) 0x55)) {
			// either one of the signifiers is missing or there aren't enough bytes for a packet
			return;
		}

		if (bytesSkipped > 0) {
			System.out.println("Skipped: " + bytesSkipped);
		}

		if (bytesSkipped > ((int) (bufferSize * 0.9))) {
			while (true) {
				System.out.println("Skipped: " + bytesSkipped);
				parent.delay(50000);
			}
		}

		// buffer is guaranteed to have the signifiers in the correct location and at
		// least 13 bytes (enough for a minimal length packet)
		int validBytesInBuffer = (bufferSize - this.bufferHead + this.bufferTail) % bufferSize;
		if (validBytesInBuffer > 70000) {
			// no need to process more than maxpacketlength
			validBytesInBuffer = 70000;
		}
		byte[] packetBuf = new byte[validBytesInBuffer];

		for (int i = 0; i < validBytesInBuffer; i++) {
			packetBuf[i] = buffer[(this.bufferHead + i) % bufferSize];
		}
		// packetBuf is now an unrolled version of the beginning of circular buffer
		// 'buffer'
		// there could be a partial packet here or an accidental lock-on to data
		if (this.s.PartialPacket(packetBuf)) {
			// System.out.println("Partial Packet Detected !");
			return;
		}

		// by checking for PartialPacket, we are now guaranteed that as this point
		// packetBuf has enough bytes to satisfy filling the packet's data buffer
		// if it's an accidental lock-on, it will now be detected as a CRC error
		// but we can't check for that, attempt to parse the packet
		int packetLength = this.s.ParsePacket(packetBuf);

		if (this.s.Verify() && (packetLength > 12)) {
			int validBytesInBuf = (bufferSize - this.bufferHead + this.bufferTail) % bufferSize;
			// packet was valid, incorporate it and update buffers
			if (packetLength > validBytesInBuf) {
				// this shouldn't be possible, several checks have been done to make sure that
				// the buffer had enough enough samples to be a full packet
				while (true) {
					System.out.println("ERROR: Valid packet causes buffer overflow");
					System.out.println("Tail:         " + this.bufferTail);
					System.out.println("Head:         " + this.bufferHead);
					System.out.println("PacketLength: " + packetLength);
					parent.delay(10000);
				}
			} else {
				this.IncorporatePacket();
				this.bufferHead = (this.bufferHead + packetLength) % bufferSize; // this shouldn't overrun due to
																					// partial packet check above
			}
		} else {
			// Something went wrong, not a valid packet
			// Either checksum was bad or header was garbled or accidental lock-on
			// but it was not a partial packet
			if (((this.bufferHead + 1) % bufferSize) != this.bufferTail) {
				this.bufferHead = (this.bufferHead + 1) % bufferSize; // move bufferHead forward one
			}
		}
	}
	
	// Return true if byteIndex was requested in the last x milliseconds
	// as determined by resendRequest___ variables
	boolean RequestedRecently(int byteIndex) {
		for(int i=0;i<resendRequestTail;i++) {
			// look for byteIndex in previous resend requests
			if( (byteIndex >= resendRequestStartBytes[i]) &&
				(byteIndex <= resendRequestEndBytes[i])	  &&
				((System.nanoTime() - resendRequestTimestamps[i]) < resendRequestMinimumGap)) {
				//System.out.println("Packet Seen Recently - Idx: " + byteIndex);
				return true;
			}
		}
		return false;
	}
	
	// Find the next gap in data that starts after byteIndex
	// If none found then return -1
	// This does not currently handle contiguous resends that are larger than 2^16-2 bytes
	int FindNextGap(int byteIndex) {
		boolean validDataFlag = false;
		for(int i=byteIndex + 1;i < this.latestValidByte; i++) {
			// if this is the first valid data we have encountered, set validDataFlag
			if(!validDataFlag && this.dataValid[i]) {
				validDataFlag = true;
			}
			if(validDataFlag && !this.dataValid[i]) {
				return i;
			}
		}
		return -1;
	}

	// Find the lowest unreceived contiguous byte interval
	// less than 2^16 (maxlen) and request it from host
	void RequestResend(Client c) {
		int start = this.dataValidUntil; // dataValidUntil guaranteed to be the lowest indexed missing byte
		if (this.dataValid[start]) {System.out.println("ERROR: Data Valid but should be Invalid");}
		// if no bytes are missing then return
		if (start >= this.latestValidByte) {return;}
		
		// if same request sent in the last x milliseconds, try to find the next gap to request
		while(this.RequestedRecently(start)) {
			start = this.FindNextGap(start);
			if(start == -1) { return; }
		}
		
		int len = 0;
		for (int i = start; i < this.latestValidByte; i++) {
			if (this.dataValid[i] || (len >= 65534)) {
				break;
			} else {
				len++;
			}
		}

		// if there is missing data, send a resend request
		if ((len > 0) && (start < this.latestValidByte)) {
			SimpleTcpNacket	nack = new SimpleTcpNacket(start, len);
			System.out.println("NCK: " + start + "/" + len);
			c.write(nack.PackNacket());
			
			// enter this request into resendRequestStartBytes etc...
			this.resendRequestStartBytes[this.resendRequestTail] = start;
			this.resendRequestEndBytes[this.resendRequestTail] = start + len - 1;
			this.resendRequestTimestamps[this.resendRequestTail++] = System.nanoTime();
		}
	}
	
	// Send the Teensy Host a RESET command
	void RequestReset(Client c) {
		SimpleTcpNacket	nack = new SimpleTcpNacket(0xFFFFDEAD, 1);
		System.out.println("SENDING RESET TO TEENSY");
		c.write(nack.PackNacket());
	}
}

// Use: n = SimpleTcpNacket(int sequenceNum, short byteLen)
//      client.write(n.PackNacket(), n.GetNacketLength())
class SimpleTcpNacket {
	byte	nackSignifier;
	int		sequenceNumber;
	short	byteLength;
	byte	checksum;

	SimpleTcpNacket() {
		this.nackSignifier	= (byte) 0xFA;
		this.sequenceNumber	= 0;
		this.byteLength		= 0;
		this.checksum		= (byte) 0xFA;
	}

	SimpleTcpNacket(int sequenceNum, int byteLen) {
		this.nackSignifier	= (byte) 0xFA;
		this.sequenceNumber	= sequenceNum;
		this.byteLength		= (short) byteLen;
		this.checksum		= this.CalculateChecksum();
	}

	byte[] PackNacket() {
		byte[] packedNack = new byte[8];
		packedNack[0]	= this.nackSignifier;
		packedNack[1]	= (byte) ((this.sequenceNumber >>> 24) & 0xFF);
		packedNack[2]	= (byte) ((this.sequenceNumber >>> 16) & 0xFF);
		packedNack[3]	= (byte) ((this.sequenceNumber >>> 8) & 0xFF);
		packedNack[4]	= (byte) ((this.sequenceNumber) & 0xFF);
		packedNack[5]	= (byte) ((this.byteLength >>> 8) & 0xFF);
		packedNack[6]	= (byte) ((this.byteLength) & 0xFF);
		packedNack[7]	= this.checksum;
		return packedNack;
	}

	int GetNacketLength() {
		return 8;
	}

	byte CalculateChecksum() {
		byte checksum = this.nackSignifier;
		checksum	^= (this.sequenceNumber & 0xFF);
		checksum	^= ((this.sequenceNumber >>> 8) & 0xFF);
		checksum	^= ((this.sequenceNumber >>> 16) & 0xFF);
		checksum	^= ((this.sequenceNumber >>> 24) & 0xFF);
		checksum	^= (this.byteLength & 0xFF);
		checksum	^= ((this.byteLength >>> 8) & 0xFF);
		return checksum;
	}
}

class SimpleTcpAcket {
	byte	ackSignifier;	// [0]
	int		sequenceNumber;	// [1-4]
	short	byteLength;		// [5-6]
	long	timestamp; 		// [7-14] 8-bytes
	byte	checksum;		// [15]

	SimpleTcpAcket() {
		this.ackSignifier	= (byte) 0xBF;
		this.sequenceNumber	= 0; // Reserved
		this.byteLength		= 0; // Reserved
		this.timestamp		= System.nanoTime();
		this.checksum		= this.CalculateChecksum();
	}

	SimpleTcpAcket(int sequenceNum, int byteLen) {
		this.ackSignifier	= (byte) 0xBF;
		this.sequenceNumber	= sequenceNum;
		this.byteLength		= (short) byteLen;
		this.timestamp		= System.nanoTime();
		this.checksum		= this.CalculateChecksum();
	}

	private byte[] PackAcket() {
		byte[] packedAck = new byte[16];
		packedAck[0]	= this.ackSignifier;
		packedAck[1]	= (byte) ((this.sequenceNumber >>> 24) & 0xFF);
		packedAck[2]	= (byte) ((this.sequenceNumber >>> 16) & 0xFF);
		packedAck[3]	= (byte) ((this.sequenceNumber >>> 8) & 0xFF);
		packedAck[4]	= (byte) ((this.sequenceNumber) & 0xFF);
		packedAck[5]	= (byte) ((this.byteLength >>> 8) & 0xFF);
		packedAck[6]	= (byte) ((this.byteLength) & 0xFF);
		packedAck[7] 	= (byte) ((this.timestamp >>> 56) & 0xFF);
		packedAck[8] 	= (byte) ((this.timestamp >>> 48) & 0xFF);
		packedAck[9] 	= (byte) ((this.timestamp >>> 40) & 0xFF);
		packedAck[10] 	= (byte) ((this.timestamp >>> 32) & 0xFF);
		packedAck[11] 	= (byte) ((this.timestamp >>> 24) & 0xFF);
		packedAck[12] 	= (byte) ((this.timestamp >>> 16) & 0xFF);
		packedAck[13] 	= (byte) ((this.timestamp >>>  8) & 0xFF);
		packedAck[14] 	= (byte) ((this.timestamp   	) & 0xFF);
		packedAck[15]	= this.checksum;
		return packedAck;
	}
	
	public void CorruptAcket() {
		this.checksum = (byte) ~this.checksum;
	}

	public int GetAcketLength() {
		return 16;
	}

	private byte CalculateChecksum() {
		byte[] ack = this.PackAcket();
		byte checksum = 0;
		for(int i=0;i<15;i++) {
			checksum ^= ack[i];
		}
		return checksum;
	}
	
	public void SendStartAcket(Client c) {
		c.write(this.PackAcket());
	}
}

class SimpleTcpPacket {
	byte	packetSignifier;
	int		sequenceNumber;
	byte	packetSignifier2;
	int		ackNumber;
	short	byteLength;
	short	checksum;
	byte[]	data;

	SimpleTcpPacket() {
		packetSignifier		= 0;
		sequenceNumber		= 0;
		packetSignifier2	= 0;
		ackNumber			= 0;
		byteLength			= 0;
		checksum			= 0;
		data				= new byte[65600];
	}

	SimpleTcpPacket(int seq, int ack, short bytel, short checks, byte[] dataIn, byte sig, byte sig2) {
		packetSignifier		= sig;
		packetSignifier2	= sig2;
		sequenceNumber		= seq;
		ackNumber			= ack;
		byteLength			= bytel;
		checksum			= checks;
		data				= new byte[65600];
		data				= dataIn.clone();
	}

	// Returns length assuming the values given are from a real packet
	int EstimatePacketLength(byte msb, byte lsb) {
		int toRet = ((msb << 8) & 0xFF00);
		toRet |= (byte) ((lsb) & 0xFF);
		return toRet + 12; // 12 header bytes
	}

	// Returns total length in bytes of the packet that started at the beginning of
	// dataIn
	int ParsePacket(byte[] dataIn) {
		this.packetSignifier = (byte) dataIn[0];
		// this.sequenceNumber = dataIn[0]<<24 & 0xFF000000;
		// this.sequenceNumber |= dataIn[1]<<16 & 0x00FF0000;
		this.sequenceNumber	= dataIn[1] << 16 & 0x00FF0000;
		this.sequenceNumber	|= dataIn[2] << 8 & 0x0000FF00;
		this.sequenceNumber	|= dataIn[3] & 0x000000FF;

		this.packetSignifier2 = (byte) dataIn[4];
		// this.ackNumber = dataIn[4]<<24 & 0xFF000000;
		// this.ackNumber |= dataIn[5]<<16 & 0x00FF0000;
		this.ackNumber	= dataIn[5] << 16 & 0x00FF0000;
		this.ackNumber	|= dataIn[6] << 8 & 0x0000FF00;
		this.ackNumber	|= dataIn[7] & 0x000000FF;

		this.byteLength	= (short) ((dataIn[8] << 8) & 0xFF00);
		this.byteLength	|= (short) (dataIn[9] & 0x00FF);

		this.checksum	= (short) ((dataIn[10] << 8) & 0xFF00);
		this.checksum	|= (short) (dataIn[11] & 0x00FF);

		for (int i = 0; i < this.byteLength; i++) {
			this.data[i] = dataIn[i + 12];
		}
		return 12 + this.byteLength;
	}

	boolean PartialPacket(byte[] d) {
		if (d.length < 13) {
			// packet not long enough to be full packet
			return true;
		}
		short byteLength = (short) ((d[8] << 8) & 0xFF00);
		byteLength |= (short) (d[9] & 0x00FF);
		int totalLength = byteLength + 12;
		if (d.length < totalLength) {
			// the array does not have enough bytes in it to be a full packet assuming the
			// header is valid
			return true;
		}
		return false;
	}

	boolean Verify() {
		// byte headerChecksum = 0;
		byte headerChecksum = this.packetSignifier;

		headerChecksum ^= this.packetSignifier2;

		headerChecksum	^= (byte) ((this.sequenceNumber) & 0xFF);
		headerChecksum	^= (byte) ((this.sequenceNumber >>> 8) & 0xFF);
		headerChecksum	^= (byte) ((this.sequenceNumber >>> 16) & 0xFF);
		// headerChecksum ^= (byte)((this.sequenceNumber>>>24) & 0xFF);

		headerChecksum	^= (byte) ((this.ackNumber) & 0xFF);
		headerChecksum	^= (byte) ((this.ackNumber >>> 8) & 0xFF);
		headerChecksum	^= (byte) ((this.ackNumber >>> 16) & 0xFF);
		// headerChecksum ^= (byte)((this.ackNumber>>>24) & 0xFF);

		headerChecksum	^= (byte) ((this.byteLength) & 0xFF);
		headerChecksum	^= (byte) ((this.byteLength >>> 8) & 0xFF);

		byte dataChecksum = 0;
		for (int i = 0; i < this.byteLength; i++) {
			dataChecksum ^= this.data[i];
		}

		int fullChecksum = ((dataChecksum << 8) & 0xFF00) | (headerChecksum & 0xFF);

		if ((fullChecksum & 0xFFFF) == (this.checksum & 0xFFFF)) {
			return true;
		} else {
			return false;
		}
	}

	void PrintInfo() {
		System.out.println("Seq#: " + this.sequenceNumber);
		System.out.println("Ack#: " + this.ackNumber);
		System.out.println("byt#: " + this.byteLength);

		if (this.Verify()) {
			System.out.println("Packet Verified");
		} else {
			System.out.println("Packet Corrupted");
		}
	}

	void PrintWithData() {
		System.out.println(String.format("0x%08X", this.sequenceNumber));
		System.out.println(String.format("0x%08X", this.ackNumber));
		System.out.println(String.format("0x%04X", this.byteLength));
		System.out.println(String.format("0x%04X", this.checksum));

		if (this.Verify()) {
			System.out.println("Packet Verified");
		} else {
			System.out.println("Packet Corrupted");
		}

		for (int i = 0; i < this.byteLength; i++) {
			System.out.print(String.format("0x%02X", this.data[i]));
			System.out.print(",");
		}
		System.out.println();
	}
}