%Modified version of the dynamic range simulation that also gives the
%variance of the SIPM signal, enabling calculation of the PTC
%Arguments:
%tau:  time constant of the SiPM cells (25.5E-9 for S14420-3025)
%npixels:  number of SiPM cells in the array (11344 for S14420-3025)

function dynamic_range_sipm_variance(tau, npixels)

%how many tau to simulate, reduces shot noise if higher
numtau = 5000;

%simulate numtau time constants worth of irradiation
time = tau*numtau;

%calculate the maximum photon flux possible (every cell firing constantly)
maxpho = npixels/tau;   %every pixel is fired once per tau
maxphotime = maxpho*time;

%number of photons per simulation duration as a function of the ratio of
%photons per SIPM time constant to SIPM cells.  Note, simluations are
%normalized to photon flux, so at high flux things become extremely slow
%(but retain constant accuracy).  
phos = logspace(log10(maxphotime/2000), log10(maxphotime/8), 40);

fprintf('Simulating %d time constants (%f seconds) from %f%% to %f%% of cells firing per recharge time (%d total)\n', numtau, numtau*tau, phos(1)/maxphotime*100, phos(end)/maxphotime*100, length(phos))

%allocate an array output currents for each flux
output =zeros(1, length(phos));

%allocate a second array to hold time resolved data, sampled at a given
%sampling rate
sample_rate = 125E6;
period = 1/sample_rate;
time_samples = time/period;

timeoutput =zeros(length(phos), ceil(time_samples)+1);

tic
%try irradating for N time constants
parfor k=1:length(phos)
    
    %allocate an array of pixel states to keep track of recharge events
    pixelarr = zeros(1,npixels);
    
    numpho = phos(k);
    
    MC_time_step = 1/(numpho/time)/10;  %step small enough that usually there is no photon since we can only simulate one per step
    
    %generate a list of photon arrival times
    event = generate_poisson_continuous(numpho/time, time, MC_time_step);
    
    %cache a list of photon arrival pixels, this is jsut for efficency
    %so we don't have to call randi in the main MC loop
    r = randi([1 npixels],1,ceil(1.5*numpho));
    
    %calculate the time constant in units of MC model time steps
    MC_tau = tau/MC_time_step;
    
    %calculate the sample period in units of MC model time steps
    MC_sample_period = period/MC_time_step;
    
    %parfor is brain dead and needs this array to parallelize
    timeoutputtmp =zeros(ceil(time_samples)+1,1);
    
    pixindex = 1;
    for i=1:length(event)
        
		%for loop because we can have more than 1 photon per time step
        %(rarely)
        for ii=1:event(i)
   
            %get a pixel from the cache
            pixnum = r(pixindex);
            pixindex=pixindex+1;
                 
            %check if the pixel is charged or not
            if(pixelarr(pixnum)==0 || i-pixelarr(pixnum)> MC_tau*5) %never hit or already recharged
                
                %store total output
                output(k) = output(k)+1;
                
                %store per sample output by dividing by the sampling rate
                %in MC time steps to get the sample index
                idx = 1+round(i/MC_sample_period);
                %timeoutput(k,idx) = timeoutput(k,idx) + 1;
                timeoutputtmp(idx)=timeoutputtmp(idx)+1;
                
            else
                %cell was partially recharged, calculate its charge
                charge = 1-exp(-1*(i-pixelarr(pixnum))/MC_tau);
                
                output(k) = output(k)+charge;
                
                %same as above, but with partial charge
                idx = 1+round(i/MC_sample_period);
                %timeoutput(k,idx) = timeoutput(k,idx) + charge;
                timeoutputtmp(idx)=timeoutputtmp(idx)+charge;
                            
                
            end
            
            %record when this pixel discharged
            pixelarr(pixnum) = i;            
            
            
        end
    end
    
    timeoutput(k,:) = timeoutputtmp;

    
end
toc
%get rid of last, truncated sample
timeoutput = timeoutput(:,1:end-1);

%calculate PTC from the time-resolved data
%sums = sum(timeoutput,2);
means = mean(timeoutput,2);
vars = var(timeoutput,[],2);

figure;plot(means, vars)
hold on; plot(means, means)
title(sprintf('Photon Transfer Curve (%d SPADs)', npixels));
xlabel('mean photons per 125 MHz A/D sample'); ylabel('photocurrent variance (units of cell gain)')

%PTC data shown per second
ft = fit(means(1:end/4), vars(1:end/4), 'poly1');
gain = ft.p1;
figure;plot(phos./time, output'./(gain)./time)
hold on; plot(phos./time,phos./time)
title(sprintf('Detected Photons from Photon Transfer Curve (%d SPADs)', npixels));
xlabel('actual photons per second'); ylabel('detected photons per second')

%true gain is 1, so any error in the PTC will change the estimate
truegain =1.0;
fprintf('Error in calculating true gain:  %f%%\n', (truegain-gain)*100)

%deviation from linear

%true gain is most relevant, calculated gain will be slightly off if you
%don't use a large number of time steps which can be very slow
figure;plot(phos./time, (phos- output./truegain)./phos*100)
hold on;plot(phos./time, (phos- output./gain)./phos*100)
title('Deviation from linear response')
legend('true gain', 'calculated gain')



