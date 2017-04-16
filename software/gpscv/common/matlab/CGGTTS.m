
classdef CGGTTS < handle
    %CGGTTS Reads a sequence of CGGTTS files
    %  Usage:
    %   CGGTTS(start MJD, stop MJD, path, file extension)
    %
    %Author: MJW 2012-11-01
    %
    %CGGTTS Properties:
    %
    %CGGTTS Methods:
    %
    %License
    %
    %The MIT License (MIT)
    %
    %Copyright (c) 2017 Michael J. Wouters
    % 
    %Permission is hereby granted, free of charge, to any person obtaining a copy
    %of this software and associated documentation files (the "Software"), to deal
    %in the Software without restriction, including without limitation the rights
    %to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    %copies of the Software, and to permit persons to whom the Software is
    %furnished to do so, subject to the following conditions:
    % 
    %The above copyright notice and this permission notice shall be included in
    %all copies or substantial portions of the Software.
    % 
    %THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    %IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    %FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    %AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    %LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    %OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    %THE SOFTWARE.

    properties
        
        Tracks; % vector of CGGTTS tracks
        Lab;
        CableDelay;
        ReferenceDelay;
        InternalDelay;
        BadTracks; % count of bad tracks flagged in the CGGTTS data
        DualFrequency;
        
        Sorted;
        
    end
    
    properties (Constant)
       % A few useful named constants
       PRN=1;
       MJD=3;
       STTIME=4;
       TRKL=5;
       ELV=6;
       AZTH=7;
       REFSV=8;
       SRSV=9;
       REFGPS=10;
       SRGPS=11;
       DSG=12;
       IOE=13;
       MDTR=14;
       SMDT=15;
       MDIO=16;
       SMDI=17;
       MSIO=18;
       SMSI=19;
       ISG=20;
    end
    
    methods (Access='public')
        % Constructor
        
        function obj=CGGTTS(startMJD,stopMJD,cctfPath,cctfExtension,removeBadTracks)
            obj.Tracks=[];
            trks=[];
            obj.DualFrequency=0;
            obj.CableDelay=0;
            obj.ReferenceDelay=0;
            obj.InternalDelay=0;
            obj.BadTracks=0;
            obj.Sorted=0;
            for mjd=startMJD:stopMJD
        
                fh = fopen([cctfPath int2str(mjd) cctfExtension]);
                
                readingHeader=1;
                while readingHeader==1
                   hdrline = fgets(fh);
                   % display(hdrline)
                   % FIXME surely you can assign within the conditional
                   % expression ? How do you do this ?
                   if (regexp(hdrline,'^\s*LAB\s*=\s*'))
                       obj.Lab=hdrline;
                       continue;
                   end;
                   
                   [mat]=regexp(hdrline,'INT\s+DLY\s*=\s*([+-]?\d+\.?\d*)','tokens');
                   if (size(mat))
                       obj.InternalDelay=str2double(mat{1});
                       continue;
                   end;
                   
                   [mat]=regexp(hdrline,'CAB\s+DLY\s*=\s*([+-]?\d+\.?\d*)','tokens');
                   if (size(mat))
                       obj.CableDelay=str2double(mat{1});
                       continue;
                   end;
                   
                   [mat]=regexp(hdrline,'REF\s+DLY\s*=\s*([+-]?\d+\.?\d*)','tokens');
                   if (size(mat))
                       obj.ReferenceDelay=str2double(mat{1});
                       continue;
                   end;
                   
                   if (regexp(hdrline,'MSIO SMSI')) % detect dual frequency
                       obj.DualFrequency=1;
                       continue;
                   end;
                   if (regexp(hdrline,'\s*hhmmss'))
                       readingHeader=0;
                       continue;
                   end
                end
               
                
                % Read the tracks
                % FIXME better to read line by line, compute the
                % checksums and warn about bad data
                if (obj.DualFrequency == 0)
                  cctftrks = fscanf(fh,'%d %x %d %s %d %d %d %d %d %d %d %d %d %d %d %d %d %x',[23 inf]);
                else
                  cctftrks = fscanf(fh,'%d %x %d %s %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %x',[26 inf]);  
                end
                cctftrks = cctftrks';
                trks=[trks;cctftrks];
                fclose(fh);
            end
            % Convert the HHMMSS [columns 4-9] field into decimal seconds
            trks(:,4)= ((trks(:,4)-48)*10 + trks(:,5)-48)*3600 + ...
                ((trks(:,6)-48)*10 + trks(:,7)-48)* 60 + ...
                (trks(:,8)-48)*10 + trks(:,9)-48;
            trks(:,5:9)=[];
            
            % Remove any bad data
            if (1==removeBadTracks)
                n = size(trks,1);
                bad = 1:n;
                badcnt=0;
                for i = 1:n
                    bad(i)=0;

                    if ((trks(i,CGGTTS.DSG) == 9999 ))
                           bad(i)=1;
                           badcnt=badcnt+1;
                    end

                    if obj.DualFrequency==1
                         % Check MSIO,SMSI,ISG for dual frequency
                         if ((trks(i,CGGTTS.ISG) == 999 ) || (trks(i,CGGTTS.MSIO) == 9999 ) || (abs(trks(i,CGGTTS.SMSI)) == 999 ))
                           bad(i)=1;
                           badcnt=badcnt+1;
                         end
                    end;

                end
                obj.BadTracks=badcnt;
                trks(any(bad,1),:)=[];
            end;
            obj.Tracks=trks;
        end
        
        function obj = FilterTracks( obj, maxDSG, minTrackLength )
            % Applies basic filtering to CGGTTS data
           
            n = size(obj.Tracks,1);
            baddsg=0;
            badtrklen=0;
            bad = 1:n;
            for i = 1:n
                bad(i)=0;
                if obj.Tracks(i,CGGTTS.DSG) >= maxDSG
                    baddsg =baddsg+1;
                    bad(i)=1;
                end
                if obj.Tracks(i,CGGTTS.TRKL) < minTrackLength
                    badtrklen = badtrklen+1;
                    bad(i)=1;
                end
            end
            
            obj.Tracks(any(bad,1),:)=[];
           
        end
       
        function Summary(obj)
            display(obj.Lab);
            display(['Cable delay =' num2str(obj.CableDelay)]);
            display(['Reference delay =' num2str(obj.ReferenceDelay)]);
            display(['Internal delay =' num2str(obj.InternalDelay)]);
            display(['Dual frequency =' num2str(obj.DualFrequency)]);
            display(['Tracks = ' num2str(size(obj.Tracks,1)) ' (' num2str(obj.BadTracks)  ' bad)']);
        end
        
        function [refgps]=AverageREFGPS(obj,useiono)
            % Calculate the averaged value of the measurand at each 
            % obervation time. Useful for REFSV etc
            
            if (obj.Sorted == 0) % only do it once
                obj.SortSVN(); 
            end;
            
            iono=1;
            if (useiono==1)
                iono=0;
            end;
            
            n = size(obj.Tracks(),1);
            i=1;
            av=0;
            cnt=0;
            sampcnt=0;
            lastmjd=obj.Tracks(i,CGGTTS.MJD);
            lastst=obj.Tracks(i,CGGTTS.STTIME);
 
            while (i<=n)
                    if (obj.Tracks(i,CGGTTS.MJD) == lastmjd && obj.Tracks(i,CGGTTS.STTIME) == lastst)
                        av = av + obj.Tracks(i,CGGTTS.REFGPS) + iono*obj.Tracks(i,CGGTTS.MDIO);
                        sampcnt=sampcnt+1;    
                    else
                        av =av/sampcnt;
                        cnt=cnt+1;
                        refgps(cnt,:) = [ lastmjd+lastst/86400.0 av ];
                        av=obj.Tracks(i,CGGTTS.REFGPS) + iono*obj.Tracks(i,CGGTTS.MDIO);
                        sampcnt=1;
                        
                    end
                    lastmjd=obj.Tracks(i,CGGTTS.MJD);
                    lastst=obj.Tracks(i,CGGTTS.STTIME);
                    i=i+1;
            end
            av =av/sampcnt; % add the last one
            cnt=cnt+1;
            refgps(cnt,:) = [lastmjd+lastst/86400.0 av];
        end
        
    end
    
    methods (Access='private')
        function SortSVN(obj)
            % Sorts tracks by SVN within each time block
            % as defined by MJD and STTIME
            % This is to make track matching easier
            n = size(obj.Tracks(),1);
            lastmjd=obj.Tracks(1,CGGTTS.MJD);
            lastst =obj.Tracks(1,CGGTTS.STTIME);
            stStart=1;
            for i=2:n
                if (obj.Tracks(i,CGGTTS.MJD) == lastmjd && ...
                        obj.Tracks(i,CGGTTS.STTIME)==lastst)
                    % sort
                    j=i;
                    while (j > stStart)
                        if (obj.Tracks(j,CGGTTS.PRN) < obj.Tracks(j-1,CGGTTS.PRN))
                            tmp = obj.Tracks(j-1,:);
                            obj.Tracks(j-1,:)= obj.Tracks(j,:);
                            obj.Tracks(j,:)=tmp;
                        else
                            break;
                        end
                        j=j-1;
                    end
                else
                    lastmjd=obj.Tracks(i,CGGTTS.MJD);
                    lastst=obj.Tracks(i,CGGTTS.STTIME);
                    stStart=i;
                    % nothing more to do
                end
            end
            obj.Sorted=1;
        end
    end
end

