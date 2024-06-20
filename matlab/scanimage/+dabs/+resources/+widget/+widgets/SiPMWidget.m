classdef SiPMWidget < dabs.resources.widget.Widget
    properties
        hAx
        hListeners = event.listener.empty(0,1);
        hSurfSipmOn
        hSurfSipmOff
        hStatusText
        hText
    end
    
    methods
        function obj = SiPMWidget(hResource,hParent)
            obj@dabs.resources.widget.Widget(hResource,hParent);
            
            obj.hListeners(end+1) = most.ErrorHandler.addCatchingListener(obj.hResource,'powerOn'      ,'PostSet',@(varargin)obj.redraw);
            obj.hListeners(end+1) = most.ErrorHandler.addCatchingListener(obj.hResource,'gain_V'       ,'PostSet',@(varargin)obj.redraw);
            obj.hListeners(end+1) = most.ErrorHandler.addCatchingListener(obj.hResource,'gainOffset_V' ,'PostSet',@(varargin)obj.redraw);
            
            try
                obj.redraw();
            catch ME
                most.ErrorHandler.logAndReportError(ME);
            end
        end
        
        function delete(obj)
            obj.hListeners.delete();
            most.idioms.safeDeleteObj(obj.hAx);
        end
    end
    
    methods
        function makePanel(obj,hParent)
            hTopFlow = most.gui.uiflowcontainer('Parent',hParent,'FlowDirection','LeftToRight','margin',0.001);
                hAxFlow = most.gui.uiflowcontainer('Parent',hTopFlow,'FlowDirection','TopDown','margin',0.001);
                hTextFlow = most.gui.uiflowcontainer('Parent',hTopFlow,'FlowDirection','TopDown','margin',0.001);
            
            obj.makeBackground(hAxFlow);
            
            obj.hText = most.gui.uicontrol('Parent',hTextFlow,'Style','text','HorizontalAlignment','left','Enable','inactive','ButtonDownFcn',@(varargin)obj.configureSipmSettings);
        end
        
        function makeBackground(obj,hParent)
            hAxFlow = most.gui.uiflowcontainer('Parent',hParent,'FlowDirection','TopDown','margin',0.001);
            obj.hAx = most.idioms.axes('Parent',hAxFlow,'Units','normalized','Position',[0 0 1 1],'DataAspectRatio',[1 1 1],'XTick',[],'YTick',[],'Visible','off','XLimSpec','tight','YLimSpec','tight');
            obj.hAx.XLim = [0.25 0.9];
            obj.hAx.YLim = [-0.02 1.02];
            view(obj.hAx,0,-90);
            
            [xx,yy,zz] = meshgrid([0 1],[0 1],0);            

            [cdata,alpha] = readIm('PMT_off.PNG',most.constants.Colors.darkGray);   % TODO: Replace PMT graphic
            obj.hSurfSipmOff     = surface('Parent',obj.hAx,'XData',xx,'YData',yy,'ZData',zz,'FaceColor','texturemap','CData',cdata,'FaceAlpha','texturemap','AlphaData',alpha,'LineStyle','none','ButtonDownFcn',@(varargin)obj.toggleOnOff);
            
            [cdata,alpha] = readIm('PMT_on.PNG',most.constants.Colors.darkGray);   % TODO: Replace PMT graphic
            obj.hSurfSipmOn      = surface('Parent',obj.hAx,'XData',xx,'YData',yy,'ZData',zz,'FaceColor','texturemap','CData',cdata,'FaceAlpha','texturemap','AlphaData',alpha,'LineStyle','none','ButtonDownFcn',@(varargin)obj.toggleOnOff);
            
            obj.hStatusText = most.util.StaticHeightText('Parent',obj.hAx,'HorizontalAlignment','center','VerticalAlignment','middle','Position',[0.5 0.84],'FontSize',0.1,'Hittest','off','PickableParts','none','FontWeight','bold','Color',most.constants.Colors.lightGray);
            
            %%% Nested function
            function [cdata,alpha] = readIm(filename,color)
                folder = fileparts(mfilename('fullpath'));
                [cdata,~,alpha] = imread(fullfile(folder,'+private',filename));
                
                cdata(:,:,1) = color(1)*255;
                cdata(:,:,2) = color(2)*255;
                cdata(:,:,3) = color(3)*255;
            end
        end
        
        function redraw(obj)
            obj.hSurfSipmOn.Visible      = 'off';
            obj.hSurfSipmOff.Visible     = 'off';
            
            if obj.hResource.powerOn
                obj.hSurfSipmOn.Visible = 'on';
                obj.hStatusText.String = 'ON';
            else
                obj.hSurfSipmOff.Visible = 'on';
                obj.hStatusText.String = 'OFF';
            end
            
            str = '';
            if ~isnan(obj.hResource.gain_V)
                str = sprintf('%s\nGain:\n  %.2fV',str,obj.hResource.gain_V);
                
                if ismethod(obj.hResource,'gainVoltsToPercent')
                    gainPercent = obj.hResource.gainVoltsToPercent(obj.hResource.gain_V);
                    str = sprintf('%s\n%.2f%%',str,gainPercent);
                end                
            end
            
            if ~isnan(obj.hResource.gainOffset_V)
                str = sprintf('%s\nOffset:\n  %.2fV',str,obj.hResource.gainOffset_V);
            end
            
            obj.hText.String = str;
        end
        
        function toggleOnOff(obj)
            try
                obj.hResource.setPower(~obj.hResource.powerOn);
            catch ME
                most.ErrorHandler.logAndReportError(ME);
            end
        end
        
        function configureSipmSettings(obj)
            opts = struct;
            opts.WindowStyle = 'normal';
            
            queries = {'SiPM Gain Voltage [V]','SiPM Offset [V]'};
            queryDefaultValues = {num2str(obj.hResource.gain_V) num2str(obj.hResource.gainOffset_V)};
            fieldWidth = [1 50];
            
            answer = most.gui.inputdlgCentered(queries,'SiPM Configuration',repmat(fieldWidth,numel(queries),1),queryDefaultValues,opts);
            
            if ~isempty(answer)
                gain_V = str2double(answer{1});
                offset_V = str2double(answer{2});
                
                try
                    obj.hResource.setGain(gain_V);
                    obj.hResource.setGainOffset(offset_V);
                catch ME
                    most.ErrorHandler.logAndReportError(ME);
                end
            end
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
