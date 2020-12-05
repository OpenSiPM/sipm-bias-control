%generate shot noise sampled at some interval
%arguments:
%lambda:  number of photons per second in expectation
%T:  simulation duration (in seconds)
%delta:  simulation step size (should be much smaller than 1/lambda)
%out:  event is an array of photons per arrival time, note that this value
%can be any non-negative integer due to oversampling

function event = generate_poisson_continuous(lambda, T, delta)

N=round(T/delta); % number of simulation steps that are output
event=zeros(N,1); % initialize counters to zeros

%upsample our dice roll so that our arrival times are (very nearly)
%uncorrelated
upsamp = 100;
delta2 = delta/upsamp;

for i=1:N
    R=rand(upsamp,1);
    event(i)=sum(R<lambda*delta2);
    
    %todo:  fully vectorize this so that we don't call rand so many times
    % for i=1:4:N
    %   R=rand(upsamp*4,1);
    %   event(i)=sum(R(1:end/4)<lambda*delta2);
    %   %event(i+1)=sum(R(end/2+1:end)<lambda*delta2);
    %   event(i+1)=sum(R(end/4+1:end/2)<lambda*delta2);
    %   event(i+2)=sum(R(end/2+1:3*end/4)<lambda*delta2);
    %   event(i+3)=sum(R(3*end/4+1:end)<lambda*delta2);
    % end
    
    % R=rand(size(event)); % generate a random array with size as "event"
    % event(R<lambda*delta)=1; % event occur if R < lambda*delta
end
