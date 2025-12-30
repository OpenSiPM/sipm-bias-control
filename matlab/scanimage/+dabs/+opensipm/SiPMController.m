classdef SiPMController < dabs.resources.Device & dabs.resources.configuration.HasConfigPage & most.HasMachineDataFile
    properties (SetAccess=protected)
        ConfigPageClass = 'dabs.resources.configuration.resourcePages.OpenSipmControllerPage';
    end
    
    methods (Static)
        function names = getDescriptiveNames()
            names = {'OpenSiPM\OpenSiPM SiPM Controller' 'PMT\OpenSiPM SiPM Controller'};  % Also show up in the PMT group
        end
    end
    %%% Abstract property realizations (most.HasMachineDataFile)
    properties (Constant, Hidden)
        %Value-Required properties
        mdfClassName = mfilename('class');
        mdfHeading = 'OpenSiPM SiPM Controller';
        
        %Value-Optional properties
        mdfDependsOnClasses; %#ok<MCCPI>
        mdfDirectProp; %#ok<MCCPI>
        mdfPropPrefix; %#ok<MCCPI>
        
        mdfDefault = defaultMdfSection();
    end
    
 %% User Props    
    %% CLASS-SPECIFIC PROPERTIES
    properties (Constant,Hidden)
        SERIAL_BAUD_RATE   = 115200;
        SERIAL_TERMINATOR  = newline;
        SERIAL_TIMEOUT     = 1;
    end
    
    %%% ABSTRACT PROPERTY REALIZATIONS (scaniamge.interfaces.SipmController)
    properties (Constant, Hidden)
        numSipms = 1;                    % [numerical] number of SiPMs managed by the SiPM controller -- one per com port
    end
    
    properties (SetObservable)
        hCOM = dabs.resources.Resource.empty();
    end

    %% Internal Properties
    properties (SetAccess = private, Hidden)
        hSerial = [];
        hSiPMs = dabs.opensipm.sipmcontroller.SiPM.empty();
        TimeoutTimer = [];
        
        replyPending = false;
        commandQueue = {};
        asyncCallback;
        lastCmd = '';
        
        model = '';
        manufacturer = '';
        serialNumber = '';
        firmware = '';
        UID = '';
        desc = '';
        mode = [];
    end
    
    %% Lifecycle
    methods
        function obj = SiPMController(name)
            obj@dabs.resources.Device(name);
            obj@most.HasMachineDataFile(true);
            
            for sipmNum = 1:obj.numSipms
                obj.hSiPMs(sipmNum) = dabs.opensipm.sipmcontroller.SiPM(obj,sipmNum);
            end
            
            obj.deinit();
            obj.loadMdf();
            obj.reinit();
        end
        
        function delete(obj)
            obj.deinit();
            obj.saveCalibration();     
            
            most.idioms.safeDeleteObj(obj.hSiPMs);
            obj.hSiPMs = dabs.opensipm.sipmcontroller.SiPM.empty();
        end
    end
    
    methods
        function loadMdf(obj)
            success = true;
            success = success & obj.safeSetPropFromMdf('hCOM', 'comPort');
            
            success = success & obj.loadCalibration();
            
            if ~success
                obj.errorMsg = 'Error loading config';
            end
        end
        
        function success = loadCalibration(obj)
            success = true;
        end
        
        function saveMdf(obj)
            obj.safeWriteVarToHeading('comPort', obj.hCOM);
            obj.saveCalibration();
        end
        
        function saveCalibration(obj)
        end
    end
    
    methods
        function deinit(obj)
            obj.errorMsg = 'Uninitialized';
            
            for sipmNum = 1:numel(obj.hSiPMs)
                hSiPM = obj.hSiPMs(sipmNum);
                if most.idioms.isValidObj(hSiPM)
                    hSiPM.deinit();
                end
            end
            
            most.idioms.safeDeleteObj(obj.TimeoutTimer);
            obj.TimeoutTimer = [];
            
            if most.idioms.isValidObj(obj.hSerial)
                fclose(obj.hSerial);
            end
            most.idioms.safeDeleteObj(obj.hSerial);
            obj.hSerial = [];
            
            obj.hCOM.unreserve(obj);
        end
        
        function reinit(obj)
            obj.deinit();
            
            try
                assert(most.idioms.isValidObj(obj.hCOM),'No serial port is specified');                
                obj.hCOM.reserve(obj);
                
                obj.hSerial = serial(obj.hCOM.name, 'BaudRate',obj.SERIAL_BAUD_RATE, 'Terminator', obj.SERIAL_TERMINATOR);
                obj.hSerial.BytesAvailableFcnMode = 'terminator';
                obj.hSerial.BytesAvailableFcn = @obj.replyAvailable;
                fopen(obj.hSerial);
                obj.TimeoutTimer = timer('Name','OpenSiPM MDU: Async Cmd Timout Timer');
                obj.TimeoutTimer.ExecutionMode = 'singleShot';
                obj.TimeoutTimer.StartDelay = obj.SERIAL_TIMEOUT;
                obj.TimeoutTimer.TimerFcn = @obj.TimeoutFcn;
                
                flushinput(obj.hSerial);
                try
                    fprintf(obj.hSerial, 'name?');
                    response = fgetl(obj.hSerial);
                catch
                    response = [];
                end
                
                if ~isempty(response) && strcmp(strtok(response), 'SiPM')
                    fprintf(1,'Done! %s', response);
                    obj.model = response;
                    obj.manufacturer = 'OpenSiPM';
                else
                    fprintf(1,'Initialization Failed! Response failed: "%s"\n', response);
                    error('\nInitialization Failed! This not a OpenSiPM SiPM Controller or it is not powered on.');
                end
                
                try
                    fprintf(obj.hSerial, 'SERIAL');
                    obj.serialNumber = fgetl(obj.hSerial);
%                     error('not implemented'); % TODO: implement device serial number in firmware
                catch
                    obj.serialNumber = [];
                    warning('Failed to get MDU Serial #. Device connection suspect!');
                end
                
                try
                    fprintf(obj.hSerial, 'VER');
                    obj.firmware = fgetl(obj.hSerial);
%                     error('not implemented'); % TODO: implement firmware version number
                catch
                    obj.firmware = [];
                    warning('Failed to get MDU Firmware Ver. Device connection suspect!');
                end
                
                try
                    fprintf(obj.hSerial, 'UID');
                    obj.UID = fgetl(obj.hSerial);
%                     error('not implemented'); % TODO: implement device UID number in firmware
                catch
                    obj.UID = [];
                    warning('Failed to get MDU UID. Device connection suspect!');
                end
                
                try
                    fprintf(obj.hSerial, 'DESC');
                    obj.desc = fgetl(obj.hSerial);
%                     error('not implemented'); % TODO: implement description
                catch
                    obj.desc = [];
                    warning('Failed to get MDU Description. Device connection suspect!');
                end
                
                obj.errorMsg = ''; % in case writeCommand checks for errors
                
                for sipmNum = 1:numel(obj.hSiPMs)
                    obj.hSiPMs(sipmNum).reinit();
                end
                
            catch ME
                obj.deinit();
                obj.errorMsg = sprintf('%s: initialization error: %s',obj.name,ME.message);
                most.ErrorHandler.logError(ME,obj.errorMsg);
            end
        end
    end
    %% USER METHODS
    methods
        function queryStatus(obj)
%             fprintf('queryStatus...\n');
            % enable
            obj.writeCommand('on?',@obj.processSiPMUpdate);
            
            % gain
            obj.writeCommand('voltage?',@obj.processSiPMUpdate);
            
            % offset
            obj.writeCommand('offset_voltage?',@obj.processSiPMUpdate);
        end
    end
    
    %% Internal methods
    methods (Hidden)        
        function TimeoutFcn(obj, ~,~)
            if obj.hSerial.BytesAvailable
                obj.replyAvailable();
            else
                stop(obj.TimeoutTimer);
                most.idioms.warn(['Timeout occurred while waiting for reply to ''' obj.lastCmd ''' cmd from OpenSiPM MDU']);
                obj.replyPending = false;
                obj.lastCmd = '';
                pause(obj.SERIAL_TIMEOUT);
                flushinput(obj.hSerial);
                
                % send next command in commandQueue
                obj.sendNextCmd();
            end
        end
        
        function writeCommand(obj, cmd, callback)
            obj.assertNoError();
            assert(isa(cmd,'char'));
           
            obj.commandQueue{end + 1} = {cmd, callback};
            obj.sendNextCmd();
        end
        
        function sendNextCmd(obj)
            if ~obj.replyPending
                % send next command in commandQueue
                if ~isempty(obj.commandQueue)
                    nextCommand = obj.commandQueue{1};
                    obj.commandQueue(1) = [];
                    
                    obj.lastCmd = nextCommand{1};
                    obj.asyncCallback = nextCommand{2};
                    
                    flushinput(obj.hSerial);
                    fprintf(obj.hSerial, obj.lastCmd);
                    stop(obj.TimeoutTimer);
                    start(obj.TimeoutTimer);
                    if strcmp(obj.lastCmd, 'RESTART')  % FIXME: "RESTART" is not implemented
                        obj.replyPending = false;
                        pause(5.5);
%                         fprintf(1, '...Done\n');
                    else
                        obj.replyPending = true;
                    end
                    
                end
            end
        end
        
        function replyAvailable(obj,~,~)
            
            try
                if strcmp(obj.lastCmd, 'RESTART')  % FIXME: "RESTART" is not implemented
                    obj.replyPending = false;
                    obj.commandQueue = {};
                    flushinput(obj.hSerial);
                end
                
                if obj.replyPending
                    stop(obj.TimeoutTimer);
                    reply = fgetl(obj.hSerial);
                    obj.replyPending = false;

                    % process answer
                    if ~isempty(obj.asyncCallback)
                        obj.asyncCallback(reply);
                        obj.asyncCallback = [];
                    end
                    obj.lastCmd = '';
                    obj.replyPending = false;
                end
                
                obj.sendNextCmd();
            catch ME
                ME.stack(1)
                fprintf(2,'Error while processing response from SiPM: %s\n', ME.message);
            end
        end
        
        function processSiPMUpdate(obj,reply)
            switch(obj.lastCmd)
                case 'on?'
                    for idx = 1:numel(obj.hSiPMs)
                        if ~isempty(reply) && strcmp(strtok(reply), 'on')
                            obj.hSiPMs(idx).setProp('powerOn', 1);
                        else
                            obj.hSiPMs(idx).setProp('powerOn', 0);
                        end
                    end
                case 'voltage?'
                    gain_voltage = str2double(reply) / 1000; % mV->V; relies on calibration stored on device
                    obj.hSiPMs(1).setProp('gain_V',gain_voltage);
%                     fprintf('setProp: gain_V=%f\n', gain_voltage);

                case 'offset_voltage?'
                    offset_voltage = str2double(reply) / 1000; %  mV->V; relies on calibration stored on device
                    obj.hSiPMs(1).setProp('gainOffset_V',offset_voltage);
%                     fprintf('setProp: gainOffset_V=%f\n', offset_voltage);
            end
        end
    end
    
    methods
        function set.hCOM(obj,val)
            if isnumeric(val) && ~isempty(val)
                val = sprintf('COM%d',val);
            end
            
            val = obj.hResourceStore.filterByName(val);
            
            if ~isequal(val,obj.hCOM)
                if most.idioms.isValidObj(val)
                    validateattributes(val,{'dabs.resources.SerialPort'},{'scalar'});
                end
                
                obj.deinit();
                obj.hCOM.unregisterUser(obj);
                obj.hCOM = val;
                val.registerUser(obj,'COM Port');
            end
        end
    end
end

function s = defaultMdfSection()
    s = [...
            most.HasMachineDataFile.makeEntry('comPort','','Serial port the SiPM is connected to (e.g. ''COM3'')')...
        ];
end



% ----------------------------------------------------------------------------
% Copyright (C) 2021 Vidrio Technologies, LLC
% 
% ScanImage (R) 2021 is software to be used under the purchased terms
% Code may be modified, but not redistributed without the permission
% of Vidrio Technologies, LLC
% 
% VIDRIO TECHNOLOGIES, LLC MAKES NO WARRANTIES, EXPRESS OR IMPLIED, WITH
% RESPECT TO THIS PRODUCT, AND EXPRESSLY DISCLAIMS ANY WARRANTY OF
% MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
% IN NO CASE SHALL VIDRIO TECHNOLOGIES, LLC BE LIABLE TO ANYONE FOR ANY
% CONSEQUENTIAL OR INCIDENTAL DAMAGES, EXPRESS OR IMPLIED, OR UPON ANY OTHER
% BASIS OF LIABILITY WHATSOEVER, EVEN IF THE LOSS OR DAMAGE IS CAUSED BY
% VIDRIO TECHNOLOGIES, LLC'S OWN NEGLIGENCE OR FAULT.
% CONSEQUENTLY, VIDRIO TECHNOLOGIES, LLC SHALL HAVE NO LIABILITY FOR ANY
% PERSONAL INJURY, PROPERTY DAMAGE OR OTHER LOSS BASED ON THE USE OF THE
% PRODUCT IN COMBINATION WITH OR INTEGRATED INTO ANY OTHER INSTRUMENT OR
% DEVICE.  HOWEVER, IF VIDRIO TECHNOLOGIES, LLC IS HELD LIABLE, WHETHER
% DIRECTLY OR INDIRECTLY, FOR ANY LOSS OR DAMAGE ARISING, REGARDLESS OF CAUSE
% OR ORIGIN, VIDRIO TECHNOLOGIES, LLC's MAXIMUM LIABILITY SHALL NOT IN ANY
% CASE EXCEED THE PURCHASE PRICE OF THE PRODUCT WHICH SHALL BE THE COMPLETE
% AND EXCLUSIVE REMEDY AGAINST VIDRIO TECHNOLOGIES, LLC.
% ----------------------------------------------------------------------------
