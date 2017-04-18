


classdef MergedCGGTTS < matlab.mixin.Copyable
    %MergedCGGTTS Groups two CGGTTS objects so that they can be merged and
    % manipulated as a single object
    %  Typical usage:
    %   mcctf = MergedCGGTTS(cctf1,cctf2)
    %   mcctf.Merge()
    %   refsv=mcctf.RefSV(1,0) % zero baseline
    %
    %
    %MergedCGGTTS Properties:
    %
    %MergedCGGTTS Methods:
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
        rx1; % CGGTTS for receiver 1
        rx2; % CGGTTS for receiver 2
        merged;
    end
    
    methods (Access='public')
         % Constructor
        
        function obj=MergedCGGTTS(ref,cal)     
            obj.rx1 = copy(ref);
            obj.rx2 = copy(cal);
            obj.merged=0;
        end
           
        function [rx1Matches,rx2Matches] = Merge(obj)
            
            % FIXME maybe check time-ordering is OK
            
            % Sort in SVN order within a track time
            
            obj.SortSVN(obj.rx1);
            obj.SortSVN(obj.rx2);
            
            n1 = size(obj.rx1.Tracks(),1);
            n2 = size(obj.rx2.Tracks(),1);
            
            rx1Matches=ones(n1,1);
            rx2Matches=ones(n2,1);
              
            i=1;
            j=1;
            
            while (i<=n1)
                mjd1=obj.rx1.Tracks(i,CGGTTS.MJD);
                st1 =obj.rx1.Tracks(i,CGGTTS.STTIME);
                prn1=obj.rx1.Tracks(i,CGGTTS.PRN);
                while (j<=n2)
                    mjd2=obj.rx2.Tracks(j,CGGTTS.MJD);
                    st2 =obj.rx2.Tracks(j,CGGTTS.STTIME);
                    if (mjd2 > mjd1)    
                       break;% stop searching - need to move pointer1
                    elseif (mjd2 < mjd1)
                        j=j+1; % need to move pointer2 ahead
                    elseif  (st2>st1) % MJDs must be same
                        break; % stop searching - need to move pointer2
                    elseif ((mjd1==mjd2) && (st1 == st2))
                        % Times are matched so search for the track
                        prn2 =obj.rx2.Tracks(j,CGGTTS.PRN);
                        while ((prn1 > prn2) && (mjd1 == mjd2) && (st1 == st2) && (j<n2))
                            j=j+1;
                            mjd2=obj.rx2.Tracks(j,CGGTTS.MJD);
                            st2 =obj.rx2.Tracks(j,CGGTTS.STTIME);
                            prn2=obj.rx2.Tracks(j,CGGTTS.PRN);
                        end
                        if ((prn1 == prn2) && (mjd1 == mjd2) && (st1 == st2) && (j<=n2))
                            % It's a match
                            rx1Matches(i)=0;
                            rx2Matches(j)=0; 
                            j=j+1;
                            break;
                        else
                            % no match so move to next i
                            break;
                        end;
                    else
                        j=j+1;
                    end
                end
                i=i+1;
            end
            obj.merged=1;
            % Remove unmatched tracks
            obj.rx1.Tracks(any(rx1Matches,2),:)=[];
            obj.rx2.Tracks(any(rx2Matches,2),:)=[]; 
        end
        
        function [refsv] = RefSV(obj,weightedMean,useIono)
            % Return (time,REF-SV)
            % Set weightedMean = 1 to get a DSG-weighted mean at 
            % each track time. Otherwise, there will be multiple tracks
            % at each observation
            
            if (obj.merged==0)
                display('Error! Not merged yet');
                return;
            end
            
            iono=1;
            if (useIono > 0)
                iono=0; 
            end;
            
            % Don't need to check array sizes - they must be the same
            if (0==weightedMean)
                refsv=[obj.rx1.Tracks(:,CGGTTS.MJD)+obj.rx1.Tracks(:,CGGTTS.STTIME)/86400.0 ... 
                      (obj.rx1.Tracks(:,CGGTTS.REFSV) + iono*obj.rx1.Tracks(:,CGGTTS.MDIO) ...
                    - obj.rx2.Tracks(:,CGGTTS.REFSV) - iono*obj.rx2.Tracks(:,CGGTTS.MDIO))];
            else
                n1 = size(obj.rx1.Tracks(),1);
                i=1;
                av=0;
                cnt=0;
                sampcnt=0;
                lastmjd=obj.rx1.Tracks(i,CGGTTS.MJD);
                lastst=obj.rx1.Tracks(i,CGGTTS.STTIME);
 
                while (i<=n1)
                    if (obj.rx1.Tracks(i,CGGTTS.MJD) == lastmjd && obj.rx1.Tracks(i,CGGTTS.STTIME) == lastst)
                        av = av + (obj.rx1.Tracks(i,CGGTTS.REFSV) + iono*obj.rx1.Tracks(i,CGGTTS.MDIO) ...
                             - obj.rx2.Tracks(i,CGGTTS.REFSV) - iono*obj.rx2.Tracks(i,CGGTTS.MDIO));
                        sampcnt=sampcnt+1;    
                    else
                        av =av/sampcnt;
                        cnt=cnt+1;
                        refsv(cnt,:) = [ lastmjd+lastst/86400.0 av ];
                        av=(obj.rx1.Tracks(i,CGGTTS.REFSV) + iono*obj.rx1.Tracks(i,CGGTTS.MDIO) ...
                             - obj.rx2.Tracks(i,CGGTTS.REFSV) - iono*obj.rx2.Tracks(i,CGGTTS.MDIO));
                        sampcnt=1;
                        
                    end
                    lastmjd=obj.rx1.Tracks(i,CGGTTS.MJD);
                    lastst=obj.rx1.Tracks(i,CGGTTS.STTIME);
                    i=i+1;
                end
                av =av/sampcnt;
                cnt=cnt+1;
                refsv(cnt,:) = [lastmjd+lastst/86400.0 av];
            end
        end
        
    end
    
    methods (Access='private')
        function SortSVN(obj,rx)
            % Sorts a CGGTTS objects tracks by SVN within each time block
            % as define by MJD and STTIME
            % This is to make track matching easier
            n = size(rx.Tracks(),1);
            lastmjd=rx.Tracks(1,CGGTTS.MJD);
            lastst =rx.Tracks(1,CGGTTS.STTIME);
            stStart=1;
            for i=2:n
                if (rx.Tracks(i,CGGTTS.MJD) == lastmjd && ...
                        rx.Tracks(i,CGGTTS.STTIME)==lastst)
                    % sort
                    j=i;
                    while (j > stStart)
                        if (rx.Tracks(j,CGGTTS.PRN) < rx.Tracks(j-1,CGGTTS.PRN))
                            tmp = rx.Tracks(j-1,:);
                            rx.Tracks(j-1,:)=rx.Tracks(j,:);
                            rx.Tracks(j,:)=tmp;
                        else
                            break;
                        end
                        j=j-1;
                    end
                else
                    lastmjd=rx.Tracks(i,CGGTTS.MJD);
                    lastst=rx.Tracks(i,CGGTTS.STTIME);
                    stStart=i;
                    % nothing more to do
                end
            end
        end
    end
    
    
end

