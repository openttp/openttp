%
%

% Data from GMAU0660.000

%% G01

% Reported by v1.0.1
% 1	00:00	01:42	1h 41m	204	498	619033	4	0	0
% 2	17:54	23:59	6h 5m	731	498	618841	18	0	0


%% G02 track 1
trks = load('G02.track1.dat');

% Checks
fprintf('Track start %06d\n',trks(1,2));
fprintf('Track stop  %06d\n',trks(end,2));
fprintf('N measurements %d\n',length(trks));

trks(:,5)= trks(:,5)/10.0;
trks(:,6)= trks(:,6)/10.0;

avREFGPS=mean(trks(:,6));


avREFSV = mean(trks(:,5));
stdREFSV= ceil(std(trks(:,5)));

fprintf('av REFGPS = %d\n',round(avREFGPS));
fprintf('av REFSV  = %d\n',round(avREFSV));
fprintf('std REFSV = %d\n',stdREFSV);

outliers = abs(trks(:,5) - avREFSV) > 4.0*stdREFSV;
fprintf('outliers > 4 sd = %d\n',sum(outliers));

outliers = abs(trks(:,5) - avREFSV) > 500.0;
fprintf('outliers > 500 ns = %d\n',sum(outliers));


%% G02 track 2
trks = load('G02.track2.dat');

% Checks
fprintf('Track start %06d\n',trks(1,2));
fprintf('Track stop  %06d\n',trks(end,2));
fprintf('N measurements %d\n',length(trks));

trks(:,5)= trks(:,5)/10.0;
trks(:,6)= trks(:,6)/10.0;

avREFGPS=mean(trks(:,6));


avREFSV = mean(trks(:,5));
stdREFSV= ceil(std(trks(:,5)));

fprintf('av REFGPS = %d\n',round(avREFGPS));
fprintf('av REFSV  = %d\n',round(avREFSV));
fprintf('std REFSV = %d\n',stdREFSV);

outliers = abs(trks(:,5) - avREFSV) > 4.0*stdREFSV;
fprintf('outliers > 4 sd = %d\n',sum(outliers));

outliers = abs(trks(:,5) - avREFSV) > 500.0;
fprintf('outliers > 500 ns = %d\n',sum(outliers));



%% G26

% Reported by V1.0.1
% 1	04:59	09:47	4h 48m	577	498	-241249	3	3	0
% 2	20:43	23:59	3h 16m	394	494	-241265	3	0	0


%% G26 track 1
trks = load('G26.track1.dat');

% Checks
fprintf('Track start %06d\n',trks(1,2));
fprintf('Track stop  %06d\n',trks(end,2));
fprintf('N measurements %d\n',length(trks));

trks(:,5)= trks(:,5)/10.0;
trks(:,6)= trks(:,6)/10.0;

avREFGPS=mean(trks(:,6));


avREFSV = mean(trks(:,5));
stdREFSV= ceil(std(trks(:,5)));

fprintf('av REFGPS = %d\n',round(avREFGPS));
fprintf('av REFSV  = %d\n',round(avREFSV));
fprintf('std REFSV = %d\n',stdREFSV);

outliers = abs(trks(:,5) - avREFSV) > 4.0*stdREFSV;
fprintf('outliers > 4 sd = %d\n',sum(outliers));

outliers = abs(trks(:,5) - avREFSV) > 500.0;
fprintf('outliers > 500 ns = %d\n',sum(outliers));


%% G26 track 2
trks = load('G26.track2.dat');

% Checks
fprintf('Track start %06d\n',trks(1,2));
fprintf('Track stop  %06d\n',trks(end,2));
fprintf('N measurements %d\n',length(trks));

trks(:,5)= trks(:,5)/10.0;
trks(:,6)= trks(:,6)/10.0;

avREFGPS=mean(trks(:,6));


avREFSV = mean(trks(:,5));
stdREFSV= ceil(std(trks(:,5)));

fprintf('av REFGPS = %d\n',round(avREFGPS));
fprintf('av REFSV  = %d\n',round(avREFSV));
fprintf('std REFSV = %d\n',stdREFSV);

outliers = abs(trks(:,5) - avREFSV) > 4.0*stdREFSV;
fprintf('outliers > 4 sd = %d\n',sum(outliers));

outliers = abs(trks(:,5) - avREFSV) > 500.0;
fprintf('outliers > 500 ns = %d\n',sum(outliers));


