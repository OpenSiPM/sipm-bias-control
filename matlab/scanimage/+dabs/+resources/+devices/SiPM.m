classdef SiPM < dabs.resources.Device & dabs.resources.widget.HasWidget
    properties (SetAccess = protected)
        WidgetClass = 'dabs.resources.widget.widgets.SiPMWidget';
    end
    
    properties (Abstract, SetAccess = protected, Hidden)
        lastQuery;   % time of last sipm status query (tic)
    end
    
    %% FRIEND PROPS
    properties (Abstract, SetAccess=protected, AbortSet, SetObservable)
        powerOn;        % [logical]   scalar containing power status for each SiPM
        gain_V;         % [numerical] scalar containing gain setting for each SiPM
        gainOffset_V;   % [numeric]   scalar containing offset for each SiPM
    end

    
    %% USER METHODS
    methods (Abstract)
        setPower(obj,tf);
        setGain(obj,gain_V);
        setGainOffset(obj,offset_V);
        
        queryStatus(obj);   % requests the SiPM controller to update its properties
    end
    
    %% Local functions
    properties (SetAccess=protected,GetAccess=protected)
        hSystemTimerListener = event.listener.empty(0,1);
    end    
    
    %% LifeCycle
    methods
        function obj = SiPM(name)
            obj@dabs.resources.Device(name);
            obj.hSystemTimerListener = most.ErrorHandler.addCatchingListener(obj.hResourceStore.hSystemTimer,'beacon_1Hz',@(varargin)obj.queryStatus);
        end
        
        function delete(obj)
            obj.hSystemTimerListener.delete();
        end
    end
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
