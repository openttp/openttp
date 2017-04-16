classdef CCTF < handle
    %CCTF Reads a sequence of CCTF files
    %   Usage:
    %   CCTF(start MJD, stop MJD, path, CCTF extension)
    %   Author: MJW 2012-11-01
    
    properties
        
        Tracks; % vector of CCTF tracks
        Lab;
        CableDelay;
        ReferenceDelay;
        InternalDelay;
        BadTracks; % count of bad tracks flagged in the CCTF data
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
        
        function obj=CCTF(startMJD,stopMJD,cctfPath,cctfExtension,removeBadTracks)
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

                    if ((trks(i,CCTF.DSG) == 9999 ))
                           bad(i)=1;
                           badcnt=badcnt+1;
                    end

                    if obj.DualFrequency==1
                         % Check MSIO,SMSI,ISG for dual frequency
                         if ((trks(i,CCTF.ISG) == 999 ) || (trks(i,CCTF.MSIO) == 9999 ) || (abs(trks(i,CCTF.SMSI)) == 999 ))
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
            % Applies standard filtering to CCTF data
           
            n = size(obj.Tracks,1);
            baddsg=0;
            badtrklen=0;
            bad = 1:n;
            for i = 1:n
                bad(i)=0;
                if obj.Tracks(i,CCTF.DSG) >= maxDSG
                    baddsg =baddsg+1;
                    bad(i)=1;
                end
                if obj.Tracks(i,CCTF.TRKL) < minTrackLength
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
            lastmjd=obj.Tracks(i,CCTF.MJD);
            lastst=obj.Tracks(i,CCTF.STTIME);
 
            while (i<=n)
                    if (obj.Tracks(i,CCTF.MJD) == lastmjd && obj.Tracks(i,CCTF.STTIME) == lastst)
                        av = av + obj.Tracks(i,CCTF.REFGPS) + iono*obj.Tracks(i,CCTF.MDIO);
                        sampcnt=sampcnt+1;    
                    else
                        av =av/sampcnt;
                        cnt=cnt+1;
                        refgps(cnt,:) = [ lastmjd+lastst/86400.0 av ];
                        av=obj.Tracks(i,CCTF.REFGPS) + iono*obj.Tracks(i,CCTF.MDIO);
                        sampcnt=1;
                        
                    end
                    lastmjd=obj.Tracks(i,CCTF.MJD);
                    lastst=obj.Tracks(i,CCTF.STTIME);
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
            lastmjd=obj.Tracks(1,CCTF.MJD);
            lastst =obj.Tracks(1,CCTF.STTIME);
            stStart=1;
            for i=2:n
                if (obj.Tracks(i,CCTF.MJD) == lastmjd && ...
                        obj.Tracks(i,CCTF.STTIME)==lastst)
                    % sort
                    j=i;
                    while (j > stStart)
                        if (obj.Tracks(j,CCTF.PRN) < obj.Tracks(j-1,CCTF.PRN))
                            tmp = obj.Tracks(j-1,:);
                            obj.Tracks(j-1,:)= obj.Tracks(j,:);
                            obj.Tracks(j,:)=tmp;
                        else
                            break;
                        end
                        j=j-1;
                    end
                else
                    lastmjd=obj.Tracks(i,CCTF.MJD);
                    lastst=obj.Tracks(i,CCTF.STTIME);
                    stStart=i;
                    % nothing more to do
                end
            end
            obj.Sorted=1;
        end
    end
    
end

