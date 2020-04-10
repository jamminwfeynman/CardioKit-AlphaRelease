%% CardioKit_Comparing_Phonocardiogram.m
%% Author: Nathan Volman

% Choose the CardioKit file 
fileName = "ck2020_03_07_00_37_43.csv"; % captured at same time as nv_07_Mar_2020_00_43.SBF, has synchronization beats, notes in lab notebook
pcgChannel = 2;

% import the reference signal file
basePath = "E:\Thesis\SchedulingDefense\Slides\EMAT_PEP\MatlabCodeAndSignals\SynchronizedRefandCk\";
filePath = strcat(basePath, fileName);
muxedSignal = csvread(filePath);

if (size(muxedSignal,1) == 1)
    muxedSignal = muxedSignal';
end

Fs = muxedSignal(1,1); % This is per channel
NumChannels = muxedSignal(2,1);
secondsToRun = muxedSignal(3,1);
FsTotal = NumChannels * Fs;
SamplesPerChannel = secondsToRun*Fs;

muxedSignal(3) = [];
muxedSignal(2) = [];
muxedSignal(1) = [];

if(mod(length(muxedSignal),FsTotal) ~= 0)
    muxedSignal(end) = [];
end

signal = reshape(muxedSignal, [SamplesPerChannel, NumChannels]);

%% Read In Reference Signal SBF
filepathREF = "E:\Thesis\SchedulingDefense\Slides\EMAT_PEP\MatlabCodeAndSignals\SynchronizedRefandCk\nv_07_Mar_2020_00_43.SBF";
[Header, RefSignal, TS] = ReadSBF(filepathREF);
FsRef = Header.sample_rate;
% synchronize the Ref and Ck signals by cutting off the start
RefSignal = RefSignal(7121:end,:);
signal = signal(9838:end,:);

%Trim synchronized signals to remove setup period
TrimStart = 11100;
RefSignal = RefSignal(TrimStart:end,:);
signal = signal(TrimStart:end,:);

figure();
hold on
plot(RefSignal(:,2).*range(signal(:,2)));
plot(signal(:,2) - 2^15);

figure();
hold on
plot(RefSignal(:,1).*range(signal(:,1)));
plot(signal(:,1) - 2^15);

for channel = 1:NumChannels
    % Separate out current channel
    testSig = signal(:, channel);

    % optionally downsample to simulate lowering sample rate
    downsampleFactor = 1;
    testSig = testSig(1:downsampleFactor:end,1);
    Fs = Fs / downsampleFactor;
    
    figure();
    subplot(4,1,1);

    plot(1:length(testSig),testSig,'b');
    title(strcat("Original Signal CH: ", num2str(channel)));
    axis([0 length(testSig) 0 2^16]);
    % apply various filters
    % for ECG, can filter below ~0.5Hz and above 80/120ish, and notch 60ish
    Fc = 0.05;
    b = fir1(500, 2/Fs * Fc, 'high');
    testSig = filtfilt(b,1,testSig);
    subplot(4,1,2);
    plot(1:length(testSig),testSig,'y');
    title("HPF 0.5HZ");
    axis([0 length(testSig) 0 2^16]);

    Fc = 150;
    b = fir1(500, 2/Fs * Fc, 'low');
    testSig = filtfilt(b,1,testSig);
    subplot(4,1,3);
    plot(1:length(testSig),testSig,'c');
    title("Then LPF 150HZ"); % 150Hz is diagnostic quality
    axis([0 length(testSig) 0 2^16]);

    Fc = [59.5,60.5];
    b = fir1(500, 2/Fs * Fc,'stop');
    testSig = filtfilt(b,1,testSig);
    subplot(4,1,4);
    plot(1:length(testSig),testSig,'g');
    title("Then Notch 59.5-60.5 HZ");
    axis([0 length(testSig) 0 2^16]);
end

%%
% trim the discontinuities from the signal
ecgSignal = signal(:,1);

% remove the baseline wander from the signal
Baseline = FastBaseline(ecgSignal, Fs);
ecgSignal = ecgSignal - Baseline;

% Scale ECG Signal to Input Referred
ecgSignal = ecgSignal .* (6.46e-5); % convert to millivolts input referred
plot(ecgSignal);

% Remove PCG Baseline
pcgSignal = signal(:,pcgChannel);
BaselinePCG = FastBaseline(pcgSignal, Fs);
pcgSignal = pcgSignal - BaselinePCG;
plot(pcgSignal);

%% Setup Filters
Fc = 0.05;
FiltHighpass = fir1(500, 2/Fs * Fc, 'high');

Fc = 150;
FiltLowpass = fir1(500, 2/Fs * Fc, 'low');

Fc = [59.5,60.5];
FiltNotch = fir1(500, 2/Fs * Fc,'stop');

%% Plot ECG and PCG Comparison to reference
ecgComp = figure();
hold on;

ecgToPlot = ecgSignal;
ecgToPlot = filtfilt(FiltHighpass,1,ecgToPlot);
ecgToPlot = filtfilt(FiltLowpass,1,ecgToPlot);
ecgToPlot = filtfilt(FiltNotch,1,ecgToPlot);

plot(rescale(ecgToPlot));

RefECG = RefSignal(:,1);
RefECG = RefECG - FastBaseline(RefECG, Fs);

RefECG = filtfilt(FiltHighpass,1,RefECG);
RefECG = filtfilt(FiltLowpass,1,RefECG);
RefECG = filtfilt(FiltNotch,1,RefECG);

plot(rescale(RefECG) + .5);

pcgComp = figure();
hold on;
plot(rescale(pcgSignal));
RefPCG = RefSignal(:,2);
RefPCG = RefPCG - FastBaseline(RefPCG, Fs);
plot(rescale(RefPCG) + 1);

% Get R-wave timing for Ensemble Averaging
[Rtimings, HR, Rsamples] = Rwave(ecgSignal, Fs, true);
% 26805,39440,193020,last beat is incomplete is resettling
RinvalidTimes = [26805 39440 193020];
jitter = Fs/10;
for i=1:length(RinvalidTimes)
    index = find( (Rsamples > (RinvalidTimes(i)-jitter)) & (Rsamples < (RinvalidTimes(i)+jitter)) );
    Rtimings(index) = [];
    Rsamples(index) = [];
    if index > 1
        HR(index-1) = [];
    end
end

samplesPerBeat = round(60*Fs / mean(HR)); % number of samples per beat
samplesBack = round(samplesPerBeat * .4);
samplesForward = samplesPerBeat - samplesBack;

set(0,'defaultfigurecolor',[1 1 1]) % white background
set(0,'defaultaxesfontname','cambria math') % beautify the axes a bit
scrn_size = get(0,'ScreenSize'); % get size of screen
shrink_pct = 0.1; % shrink the figure by 10%

fig1 = figure('Visible','off','DefaultAxesFontSize',60,'Position',...
    [scrn_size(1)+(scrn_size(3)*shrink_pct) scrn_size(2)+(scrn_size(4)*shrink_pct)...
    scrn_size(3)-(scrn_size(3)*2*shrink_pct) scrn_size(4)-(scrn_size(4)*2*shrink_pct)]); % shrinking the figure

EMATm = [];
STDm = [];
RefEMATm = [];
RefSTDm = [];
BeatEstimates = [];
QTestimates = [];

for loopindex = 125: length(Rsamples)
    beatsPerAverage = loopindex;
    NumAverages  = floor(length(Rsamples) / beatsPerAverage);

    % Store chunked up ensemble averaged beats in matrix as column vectors
    ecgAveragedBeats = [];
    ecgAveragedBeatsSTD = [];
    pcgAveragedBeats = [];
    pcgAveragedBeatsSTD = [];
    
    RecgAveragedBeats = [];
    RecgAveragedBeatsSTD = [];
    RpcgAveragedBeats = [];
    RpcgAveragedBeatsSTD = [];

    for av = 0:(NumAverages-1)
        RsubSample = Rsamples(av*beatsPerAverage+1:(av+1)*beatsPerAverage);
        % Ensemble average ECG using R-wave Timings
        [AE, AEstd] = ensavg(ecgSignal, RsubSample, samplesBack, samplesForward);
        ecgAveragedBeats = [ecgAveragedBeats AE];
        ecgAveragedBeatsSTD = [ecgAveragedBeatsSTD AEstd];
        % Ensemble average PCG using R-wave Timings from ECG
        [AE2, AE2std] = ensavg(pcgSignal, RsubSample, samplesBack, samplesForward);
        pcgAveragedBeats = [pcgAveragedBeats AE2];
        pcgAveragedBeatsSTD = [pcgAveragedBeatsSTD AE2std];

        xAxisTimings = 0:1000/Fs:1000*(length(AE)-1)/Fs;
        
        % Ensemble average ECG using R-wave Timings
        [RAE, RAEstd] = ensavg(RefSignal(:,1), RsubSample, samplesBack, samplesForward);
        RecgAveragedBeats = [RecgAveragedBeats RAE];
        RecgAveragedBeatsSTD = [RecgAveragedBeatsSTD RAEstd];
        % Ensemble average PCG using R-wave Timings from ECG
        [RAE2, RAE2std] = ensavg(RefSignal(:,2), RsubSample, samplesBack, samplesForward);
        RpcgAveragedBeats = [RpcgAveragedBeats RAE2];
        RpcgAveragedBeatsSTD = [RpcgAveragedBeatsSTD RAE2std];

        figure();
        hold on;
        plot(xAxisTimings,AE);
        plot(xAxisTimings,AE2);

        title(strcat("PCG+ECG: ",num2str(av)), 'FontSize', 24);
        xlabel("Time (milliseconds)");
        ylabel("Amplitude (mV)");

        txt = ['Nbeats: ' num2str(length(RsubSample))];
        dim = [.2 .5 .3 .3];
        annotation('textbox',dim,'String',txt,'FitBoxToText','on');
        
        figure();
        hold on;
        plot(xAxisTimings,RAE);
        plot(xAxisTimings,RAE2);

        title(strcat("Ref PCG+ECG: ",num2str(av)), 'FontSize', 24);
        xlabel("Time (milliseconds)");
        ylabel("Amplitude (mV)");

        txt = ['Nbeats: ' num2str(length(RsubSample))];
        dim = [.2 .5 .3 .3];
        annotation('textbox',dim,'String',txt,'FitBoxToText','on');
    end

    %%
    % plot derivatives of sub-averages overlayed onto those sub-averages
    delECG = zeros(size(ecgAveragedBeats));
    for i = 1:NumAverages
        delECG(:,i) = derivative(ecgAveragedBeats(:,i),40,Fs);
    end
    % scale derivative (only useful for pretty plot)
    delECG = delECG .* (max(ecgAveragedBeats)/ max(delECG));

    % plot 2nd derivatives and overlay those too
    del2ECG = zeros(size(ecgAveragedBeats));
    for i = 1:NumAverages
        del2ECG(:,i) = derivative(delECG(:,i),40,Fs);
    end
    % scale derivative (only useful for pretty plot)
    del2ECG = del2ECG .* (max(ecgAveragedBeats)/ max(del2ECG));
    
    %% Now do the same for the reference ecg
    RdelECG = zeros(size(RecgAveragedBeats));
    for i = 1:NumAverages
        RdelECG(:,i) = derivative(RecgAveragedBeats(:,i),40,Fs);
    end
    % scale derivative (only useful for pretty plot)
    RdelECG = RdelECG .* (max(RecgAveragedBeats)/ max(RdelECG));

    % plot 2nd derivatives and overlay those too
    Rdel2ECG = zeros(size(RecgAveragedBeats));
    for i = 1:NumAverages
        Rdel2ECG(:,i) = derivative(RdelECG(:,i),40,Fs);
    end
    % scale derivative (only useful for pretty plot)
    Rdel2ECG = Rdel2ECG .* (max(RecgAveragedBeats)/ max(Rdel2ECG));
    
    timingFig = figure();
    hold on; 
    plot(xAxisTimings,ecgAveragedBeats,'b');
    plot(xAxisTimings,delECG,'g');
    plot(xAxisTimings,del2ECG,'r');
    title("ECG Timings");

    %%
    % plot derivatives of sub-averages overlayed onto those sub-averages
    delPCG = zeros(size(pcgAveragedBeats));
    for i = 1:NumAverages
        delPCG(:,i) = derivative(pcgAveragedBeats(:,i),40,Fs);
    end
    % scale derivative
    delPCG = delPCG .* (max(pcgAveragedBeats)/ max(delPCG));

    % plot 2nd derivatives and overlay those too
    del2PCG = zeros(size(pcgAveragedBeats));
    for i = 1:NumAverages
        del2PCG(:,i) = derivative(delPCG(:,i),40,Fs);
    end
    % scale derivative
    del2PCG = del2PCG .* (max(pcgAveragedBeats)/ max(del2PCG));
    
    %% Now do the same for the reference pcg
    RdelPCG = zeros(size(RpcgAveragedBeats));
    for i = 1:NumAverages
        RdelPCG(:,i) = derivative(RpcgAveragedBeats(:,i),40,Fs);
    end
    % scale derivative (only useful for pretty plot)
    RdelPCG = RdelPCG .* (max(RpcgAveragedBeats)/ max(RdelPCG));

    % plot 2nd derivatives and overlay those too
    Rdel2PCG = zeros(size(RpcgAveragedBeats));
    for i = 1:NumAverages
        Rdel2PCG(:,i) = derivative(RdelPCG(:,i),40,Fs);
    end
    % scale derivative (only useful for pretty plot)
    Rdel2PCG = Rdel2PCG .* (max(RpcgAveragedBeats)/ max(Rdel2PCG));

    timingFigPCG = figure();
    hold on; 
    plot(xAxisTimings,pcgAveragedBeats,'b');
    plot(xAxisTimings,delPCG,'g');
    plot(xAxisTimings,del2PCG,'r');
    title("PCG Timings");

    plot(derivative(ecgSignal,10,Fs)); 
    plot(ecgSignal);

    pcgDeriv = derivative(pcgSignal, 10, Fs);
    pcgDeriv = pcgDeriv ./max(pcgDeriv);
    pcgSignal = pcgSignal ./max(pcgSignal);
    figure
    hold on
    plot(pcgDeriv);
    plot(pcgSignal);
    
    ecgDeriv = derivative(ecgSignal, 10, Fs);
    ecgDeriv = ecgDeriv ./max(ecgDeriv);
    ecgDeriv = ecgDeriv + 1;
    ecgSignal = ecgSignal ./max(ecgSignal);
    figure
    hold on
    plot(ecgDeriv);
    plot(ecgSignal);

    %% to Find EMAT
    Qsamples = [];
    S1samples = [];
    RefQsamples = [];
    RefS1samples = [];
    Tsamples = [];
    RefTsamples = [];
    QsamplesForQT = [];
    RefQsamplesForQT = [];
    % for each sub-average
    for i = 1:NumAverages
        %% Finding Q
        tWindowBeforeR = 70; % milliseconds before R wave that Q could be
        tWindowAfterR = 35; % the R wave is at least 35 milliseconds before R (probably don't need this and wont work for monophasic R)
        tWindowBeforeRms = floor(tWindowBeforeR * Fs / 1000); % convert milliseconds to samples
        tWindowAfterRms = floor(tWindowAfterR * Fs / 1000); % convert milliseconds to samples

        % Get R-wave timing for Ensemble Averaging
        [m,Rsamples_i] = min(ecgAveragedBeats(:,i));
        [M,idx_max_del2ecg] = max( del2ECG( Rsamples_i - tWindowBeforeRms : Rsamples_i - tWindowAfterRms , i) );
        idx_max_del2ecg = idx_max_del2ecg + Rsamples_i - tWindowBeforeRms - 1; % correct for the fact that max function returns index into array fragment given not into whole array
        Qsamples = [Qsamples; idx_max_del2ecg];
        
        % Do the same for reference signal
        [Refm,RefRsamples_i] = min(RecgAveragedBeats(:,i));
        [RefM,Refidx_max_del2ecg] = max( Rdel2ECG( RefRsamples_i - tWindowBeforeRms : RefRsamples_i - tWindowAfterRms , i) );
        Refidx_max_del2ecg = Refidx_max_del2ecg + RefRsamples_i - tWindowBeforeRms - 1; % correct for the fact that max function returns index into array fragment given not into whole array
        RefQsamples = [RefQsamples; Refidx_max_del2ecg];

        %% Finding S1
        % Detect negative-going zero crossing of 2nd derivative with 15 ms
        % holdoff before R wave
        tWindowBeforeR = 15; % milliseconds before R wave that Q could be
        tWindowAfterR = 10; % the R wave is at least 35 milliseconds before R (probably don't need this and wont work for monophasic R)
        tWindowBeforeRms = floor(tWindowBeforeR * Fs / 1000); % convert milliseconds to samples
        tWindowAfterRms = floor(tWindowAfterR * Fs / 1000); % convert milliseconds to samples
        for j = Rsamples_i - tWindowBeforeRms : Rsamples_i - tWindowAfterRms
            zerocross = del2PCG(j)*del2PCG(j-1) < 0 ;
            falling = del2PCG(j) < 0 ;
            if(zerocross && falling)
                S1samples = [S1samples; j];
                break;
            end
        end
        % make sure there were valid times found for both Q and S1, if not
        % skip this average
        if (length(Qsamples) > length(S1samples))
            Qsamples(end) = [];
        elseif (length(Qsamples) < length(S1samples))
            S1samples(end) = [];
        end
        
        % Find S1 in reference signal
        for j = RefRsamples_i - tWindowBeforeRms : RefRsamples_i - tWindowAfterRms
            zerocross = Rdel2PCG(j)*Rdel2PCG(j-1) < 0 ;
            falling = Rdel2PCG(j) < 0 ;
            if(zerocross && falling)
                RefS1samples = [RefS1samples; j];
                break;
            end
        end
        % make sure there were valid times found for both Q and S1, if not
        % skip this average
        if (length(RefQsamples) > length(RefS1samples))
            RefQsamples(end) = [];
        elseif (length(RefQsamples) < length(RefS1samples))
            RefS1samples(end) = [];
        end
        
        %% Make sure that both CardioKit and Reference were valid this beat, else ignore this beat
        if (length(RefQsamples) > length(Qsamples))
            RefQsamples(end) = [];
            RefS1samples(end) = [];
        elseif (length(RefQsamples) < length(Qsamples))
            Qsamples(end) = [];
            S1samples(end) = [];
        end
        
        %% Find end of T-wave for QTc
        % Finding T
        % holdoff before R wave
        tHoldoff = 150; % milliseconds before R wave that Q could be
        tHoldoff_samples = floor(tHoldoff * Fs / 1000); % convert milliseconds to samples
        [M,    idx_max_del2ecg_T]    = max(  del2ECG( Rsamples_i    + tHoldoff_samples : end , i) );
        [RefM2,Refidx_max_del2ecg_T] = max( Rdel2ECG( RefRsamples_i + tHoldoff_samples : end , i) );
        if ( not(isempty(idx_max_del2ecg_T))    &&... 
             not(isempty(Refidx_max_del2ecg_T)) &&...
             not(isempty(idx_max_del2ecg))      &&...
             not(isempty(Refidx_max_del2ecg))     )
             
            Tsamples    = [Tsamples;    idx_max_del2ecg_T    + Rsamples_i    + tHoldoff_samples - 1];
            RefTsamples = [RefTsamples; Refidx_max_del2ecg_T + RefRsamples_i + tHoldoff_samples - 1];
            
            QsamplesForQT = [QsamplesForQT; idx_max_del2ecg];
            RefQsamplesForQT = [RefQsamplesForQT; Refidx_max_del2ecg];
        end
        RRav = 60/mean(HR);
        %% Plot representative beat with timings marked
        if( (beatsPerAverage >= 125))% && (i == 1) )
           %% Plot marked CardioKit Average Beat
           digit = 4;
           
           CkECGmarked = figure();
           hold on;
           pcgAveragedBeats(:,i) = rescale(pcgAveragedBeats(:,i));
           plot(xAxisTimings, ecgAveragedBeats(:,i));
           plot(xAxisTimings, pcgAveragedBeats(:,i));
           plot(xAxisTimings(QsamplesForQT(i)), ecgAveragedBeats(QsamplesForQT(i),i), 'ro');
           plot(xAxisTimings(S1samples(i)), pcgAveragedBeats(S1samples(i),i), 'ro');
           line([xAxisTimings(QsamplesForQT(i)),xAxisTimings(QsamplesForQT(i))],[-.5,.75],'Color','black','LineStyle','--','LineWidth',1);
           line([xAxisTimings(S1samples(i)),xAxisTimings(S1samples(i))],[-.5,.75],'Color','black','LineStyle','--','LineWidth',1);
           legend('Averaged ECG','Averaged PCG');
           xlabel('Time (ms)');
           ylabel('Scaled Amplitude');
           title('Example Q Wave and S1 Estimate: CardioKit');
           txt = ['Nbeats: ' num2str(beatsPerAverage)];
           dim = [.2 .5 .3 .3];
           annotation('textbox',dim,'String',txt,'FitBoxToText','on');
           EmatmInMilliseconds = xAxisTimings(S1samples(i)) - xAxisTimings(QsamplesForQT(i));
           txt2 = ['EMAT Estimate: ' num2str(round(EmatmInMilliseconds,digit,'significant')) ' ms'];
           annotation('textbox',dim,'String',txt2,'FitBoxToText','on');
           
           RefECGmarked = figure();
           hold on;
           RpcgAveragedBeats(:,i) = rescale(RpcgAveragedBeats(:,i));
           plot(xAxisTimings, RecgAveragedBeats(:,i));
           plot(xAxisTimings, RpcgAveragedBeats(:,i));
           plot(xAxisTimings(RefQsamplesForQT(i)), RecgAveragedBeats(RefQsamplesForQT(i),i), 'ro');
           plot(xAxisTimings(RefS1samples(i)), RpcgAveragedBeats(RefS1samples(i),i), 'ro');
           line([xAxisTimings(RefQsamplesForQT(i)),xAxisTimings(RefQsamplesForQT(i))],[-.5,.75],'Color','black','LineStyle','--','LineWidth',1);
           line([xAxisTimings(RefS1samples(i)),xAxisTimings(RefS1samples(i))],[-.5,.75],'Color','black','LineStyle','--','LineWidth',1);
           legend('Averaged ECG','Averaged PCG');
           xlabel('Time (ms)');
           ylabel('Scaled Amplitude');
           title('Example Q Wave and S1 Estimate: Reference');
           txt = ['Nbeats: ' num2str(beatsPerAverage)];
           dim = [.2 .5 .3 .3];
           annotation('textbox',dim,'String',txt,'FitBoxToText','on');
           EmatmInMilliseconds = xAxisTimings(RefS1samples(i)) - xAxisTimings(RefQsamplesForQT(i));
           txt2 = ['EMAT Estimate: ' num2str(round(EmatmInMilliseconds,digit,'significant')) ' ms'];
           annotation('textbox',dim,'String',txt2,'FitBoxToText','on');
           
           figure();
           hold on;
           r1 = plot(xAxisTimings, rescale(pcgAveragedBeats(:,i)));
           b1 = plot(xAxisTimings - 1.5, 0.75 + rescale(RpcgAveragedBeats(:,i)));
           title('Qualitative Comparison of CardioKit PCG vs. Reference');
           legend([r1 b1], {'CardioKit','Reference (Thinklabs DS32A)'});
           xlabel('Time (ms)');
           ylabel('Scaled Amplitude');
           txt = ['Nbeats: ' num2str(beatsPerAverage)];
           dim = [.2 .5 .3 .3];
           annotation('textbox',dim,'String',txt,'FitBoxToText','on');
           
           lines = [209, 228, 253, 268, 488, 508];
           for linNum = 1: length(lines)
               aa=line([lines(linNum),lines(linNum)],[.1,1.8],'Color','black','LineStyle','--','LineWidth',1);
           end
           
           RefECGmarked = figure();
           hold on;
           plot(xAxisTimings, RecgAveragedBeats(:,i));
           plot(xAxisTimings(RefQsamplesForQT(i)), RecgAveragedBeats(RefQsamplesForQT(i),i), 'ro');
           plot(xAxisTimings(RefTsamples(i)), RecgAveragedBeats(RefTsamples(i),i), 'ro');
           line([xAxisTimings(RefQsamplesForQT(i)),xAxisTimings(RefQsamplesForQT(i))],[-.5,.5],'Color','black','LineStyle','--','LineWidth',1);
           line([xAxisTimings(RefTsamples(i)),xAxisTimings(RefTsamples(i))],[-.5,.5],'Color','black','LineStyle','--','LineWidth',1);
           xlabel('Time (ms)');
           ylabel('Input Referred Amplitude (mV)');
           title('Example Q and T Wave Estimate: Reference');
           txt = ['Nbeats: ' num2str(beatsPerAverage)];
           dim = [.2 .5 .3 .3];
           annotation('textbox',dim,'String',txt,'FitBoxToText','on');
           RefQTmInMilliseconds = xAxisTimings(RefTsamples(i)) - xAxisTimings(RefQsamplesForQT(i));
           RefQTc_Bazetts = RefQTmInMilliseconds / nthroot(RRav,2);
           RefQTc_Fridericia = RefQTmInMilliseconds / nthroot(RRav,3);
           RefQTc_Sagie = RefQTmInMilliseconds + (0.154*(1-RRav));
           txt1 = ['QTc Estimate (Bazetts): ' num2str(round(RefQTc_Bazetts,digit,'significant'))];
           txt2 = ['QTc Estimate (Fridericia): ' num2str(round(RefQTc_Fridericia,digit,'significant'))];
           txt3 = ['QTc Estimate (Sagie): ' num2str(RefQTc_Sagie)];
           annotation('textbox',dim,'String',txt1,'FitBoxToText','on');
           annotation('textbox',dim,'String',txt2,'FitBoxToText','on');
           annotation('textbox',dim,'String',txt3,'FitBoxToText','on');
        end
        
        
    end
    
    % Store the estimates from both reference and ck in BeatEstimates
    BeatEstimates = [BeatEstimates ; [(mean(S1samples - Qsamples) * 1000 / Fs) (mean(RefS1samples - RefQsamples) * 1000 / Fs);]];

    % Store the estimates for QT for both reference and ck in QTestimates
    QTestimates = [QTestimates; [(mean(Tsamples - QsamplesForQT) * 1000 / Fs) (mean(RefTsamples - RefQsamplesForQT) * 1000 / Fs);]];
    
    EMAT = mean(S1samples - Qsamples) * 1000 / Fs;
    EMATstd = std(S1samples - Qsamples) * 1000 / Fs;
    RefEMAT = mean(RefS1samples - RefQsamples) * 1000 / Fs;
    RefEMATstd = std(RefS1samples - RefQsamples) * 1000 / Fs;

    EMATm = [EMATm; EMAT];
    STDm = [STDm ; EMATstd];
    RefEMATm = [RefEMATm; RefEMAT];
    RefSTDm = [RefSTDm ; RefEMATstd];
end
%%
% remove outliers from the EMAT estimate
outL = isoutlier(EMATm);
EMATm(outL) = [];

% print estimated EMAT from average of averages voting
EMATmInMilliseconds = mean(EMATm)
EmatSTDmInMilliseconds = mean(STDm)

%% Remove outliers from BeatEstimates and QTestimates
outL_BeatEstimates = isoutlier(BeatEstimates);
outL_BeatEstimates = outL_BeatEstimates(:,1) | outL_BeatEstimates(:,2);
BeatEstimates(outL_BeatEstimates, :) = [];

outL_QTEstimates = isoutlier(QTestimates);
outL_QTEstimates = outL_QTEstimates(:,1) | outL_QTEstimates(:,2);
QTestimates(outL_QTEstimates, :) = [];

BlandAltman(BeatEstimates(:,1),BeatEstimates(:,2));
BlandAltman(QTestimates(:,1),QTestimates(:,2));

%%
figure();
boxplot(BeatEstimates,'Labels',{'CardioKit','Reference'});
xlabel('CardioKit vs. Reference');
ylabel('Time (ms)');
title('EMAT Estimates CardioKit vs. Reference');

figure();
boxplot(QTestimates,'Labels',{'CardioKit','Reference'});
xlabel('CardioKit vs. Reference');
ylabel('Time (ms)');
title('QT Estimates CardioKit vs. Reference');