
%% MINI HOW TO

% % List serial ports if needed:
% disp(serialportlist("all"))

% Open serial port connection
clear all  % ensure released (if previous handle)
sipm = open_sipmdev("COM8");

% Issue commands
setint(sipm, "gain", 130); % Adjust integer value
setint(sipm, "offset", 263); % Adjust integer value
% setflt(sipm,"voltage", 42); %% failure: sets to gain 0.

% Display status (on/gain/offset)
disp(qry_status(sipm))

% Ensure release at end of script
clear sipm


%%% Ad-hoc Library Functions
function [sipmdev] = open_sipmdev(com)
    sipmdev = serialport(com,115200,"Timeout",2);
    writeline(sipmdev, "?");disp(strip(readline(sipmdev)));
end
function [msg] = qry_status(sipmdev)
    msg = sprintf("Status: %s gain=%s offset=%s",...
        qry(sipmdev,"on?"), qry(sipmdev,"gain?"), qry(sipmdev,"offset?"));
end
function [reply] = qry(com,query)
    writeline(com, query);
    reply=strip(readline(com));
end
function setint(com,name,val)
    writeline(com, sprintf("%s %d",name,val));
end
function setflt(com,name,val)
    writeline(com, sprintf("%s %f",name,val));
end
