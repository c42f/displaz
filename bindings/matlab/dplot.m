function dplot(position, varargin)
    % 3D plotting function for points and lines
    %
    % The interface is similar to plot() where possible and convenient.
    %
    % dplot(P) Plots the Nx3 array P as points, with a point in each row
    % (so X = P(:,1), Y = P(:,2), etc)
    %
    % dplot(P, S) Plots points P using the marker shape and color specified in
    % the string S.  The following characters are supported:
    %
    %      b     blue          .     sphere             -     solid
    %      g     green         o     circle           (none)  no line
    %      r     red           x     x-mark
    %      c     cyan          +     plus
    %      m     magenta       s     square
    %      y     yellow
    %      k     black
    %      w     white
    %
    % dplot(P, [S, ] 'property1', value1, ...) accepts properties and values
    % which affect the way that the data series is plotted.  Supported
    % properties are
    %    color - a 1x3 array or Nx3 array of colors per point
    %    markersize - a length 1 or N vector of marker sizes per point
    %    markershape - a length 1 or N vector of integer marker shapes per point

    if isempty(position)
        return;
    end

    global g_DisplazHold;

    color = [1 1 1];
    markersize = 0.1;
    markershape = 0;
    plotLine = false;
    plotMarkers = false;

    skipArg = false;
    for i=1:length(varargin)
        if skipArg
            skipArg = false;
            continue;
        end
        arg = varargin{i};
        if ~ischar(arg)
            error('Argument number %d to dplot not recognized', i+1)
        end
        arg = lower(arg);
        switch arg
            case 'color'
                color = varargin{i+1};
                skipArg = true;
            case 'markersize'
                markersize = ensure_vec(varargin{i+1}, 'markersize');
                skipArg = true;
            case 'markershape'
                markershape = ensure_vec(varargin{i+1}', 'markershape');
                skipArg = true;
            otherwise
                if i ~= 1
                    error('Argument "%s" to dplot unrecognized', varargin{i});
                end
                % Try to parse first argument as color/markershape/linespec
                [color, markershape, plotLine, plotMarkers] = ...
                    parse_plot_spec(arg, color, markershape, plotLine, plotMarkers);
        end
    end

    if ~plotLine && ~plotMarkers
        plotMarkers = true;
    end

    if size(position, 2) ~= 3; error('position must be a Nx3 array'); end
    if size(color, 2)    ~= 3; error('color must be a Nx3 array'); end

    nvertices = size(position, 1);
    if size(color,1) == 1
        color = repmat(color, nvertices, 1);
    end
    if length(markersize) == 1
        markersize = repmat(markersize, nvertices, 1);
    end
    if length(markershape) == 1
        markershape = repmat(markershape, nvertices, 1);
    end

    tmpLineFileName = '';
    tmpPointFileName = '';

    if plotLine
        tmpLineFileName = sprintf('%s_lines.ply',tempname);
        % Plot lines.  This is currently rather limited due to the
        % inability to specify a shader for line primitives
        write_ply_lines(tmpLineFileName, position, color);
    end
    if plotMarkers
        tmpPointFileName = sprintf('%s_points.ply',tempname);
        % Ensure all fields are floats for now, to avoid surprising scaling in the
        % shader
        if size(color,1) ~= nvertices; error('color must have same number of rows as position array'); end
        fields = struct('name',{'position','color','markersize','markershape'},...
                        'semantic',{'vector_semantic','color_semantic','array_semantic','array_semantic'},...
                        'typeName',{'float64','float32','float32','uint8'},...
                        'value',{double(position),single(color),single(markersize),uint8(markershape)}...
                        );
        write_ply_points(tmpPointFileName, nvertices,fields);
    end

    if g_DisplazHold
        holdStr = '-add';
    else
        holdStr = '';
    end
    fixLdLibPath = '';
    if isunix()
        % Reset LD_LIBRARY_PATH on unix - matlab adds to it to get the
        % matlab version of libraries, which tend to conflict with the
        % libraries used when building displaz.
        fixLdLibPath = 'env LD_LIBRARY_PATH=""';
    end
    displazCall=sprintf('%s displaz %s -shader generic_points.glsl -rmtemp %s %s &', ...
                        fixLdLibPath, holdStr, tmpPointFileName, tmpLineFileName);
    % disp(displazCall);
    system(displazCall);
    % FIXME: Calling displaz too often in a loop causes contention on the
    % socket which can cause a new instance to be launched.  Pause a bit to
    % work around this for now (ugh!)
    pause(0.01);
end


function x = ensure_vec(x, name)
    if size(x,1) == 1
        x = x';
    end
    if size(x,2) ~= 1
        error('Expected "%s" to be a vector', name);
    end
end


function write_ply_points(fileName, nvertices, fields)
    % Write a set of points to displaz-native ply format
    fid = fopen(fileName, 'w');
    fprintf(fid,...
        ['ply\n',...
        'format binary_little_endian 1.0\n',...
        'comment Displaz native\n'...
        ]);
    for item = fields
        name = item.name;
        semantic = item.semantic;
        typeName = item.typeName;
        value = item.value;
        n = size(value,1);
        assert(n == nvertices || n == 1);
        fprintf(fid, 'element vertex_%s %d\n',name,n);
        for i = 1:size(value,2)
            propName = ply_property_name(semantic, i);
            fprintf(fid, 'property %s %s\n',typeName,propName);
        end
    end
    fprintf(fid, 'end_header\n');
    for item = fields
        fwrite(fid, item.value',item.typeName,0,'ieee-le');
    end
    fclose(fid);
end


function write_ply_lines(fileName, position, color)
    vertexValid = all(~isnan(position), 2);
    lineStarts = find(diff([0;vertexValid]) == 1);
    % Write a set of points to displaz-native ply format
    fid = fopen(fileName, 'w');
    nvalidvertices = sum(vertexValid);
    fprintf(fid, ...
        ['ply\n', ...
        'format binary_little_endian 1.0\n', ...
        'element vertex %d\n', ...
        'property double x\n', ...
        'property double y\n', ...
        'property double z\n', ...
        'element color %d\n', ...
        'property float r\n', ...
        'property float g\n', ...
        'property float b\n', ...
        'element edge %d\n', ...
        'property list int int vertex_index\n', ...
        'end_header\n' ...
        ], ...
        nvalidvertices, ...
        nvalidvertices, ...
        length(lineStarts) ...
    );
    fwrite(fid, position(vertexValid,:)', 'double', 0, 'ieee-le');
    fwrite(fid, color(vertexValid,:)', 'float', 0, 'ieee-le');
    % Write out line connectivity
    nvertices = size(position,1);
    realStart = 0;
    for i = lineStarts'
        j = i;
        while j <= nvertices && vertexValid(j)
            j = j + 1;
        end
        lineLen = j-i;
        fwrite(fid, lineLen, 'int32');
        fwrite(fid, realStart:realStart+lineLen-1, 'int32', 0, 'ieee-le');
        realStart = realStart + lineLen;
    end
    fclose(fid);
end


function plyProperty = ply_property_name(semantic, idx)
    if strcmp(semantic,'array_semantic')
        plyProperty = num2str(idx-1);
    elseif strcmp(semantic,'vector_semantic')
        vector_semantic = ['x', 'y', 'z', 'w'];
        plyProperty = vector_semantic(idx);
    elseif strcmp(semantic, 'color_semantic')
        color_semantic = ['r', 'g', 'b'];
        plyProperty = color_semantic(idx);
    end
end


function [color, markerShape, plotLine, plotMarkers] = parse_plot_spec(...
        spec, defaultColor, defaultShape, defaultPlotLine, defaultPlotMarkers)
    color       = defaultColor;
    markerShape = defaultShape;
    plotLine    = defaultPlotLine;
    plotMarkers = defaultPlotMarkers;
    for s = spec(:)'
        switch s
            % Colors
            case 'r'
                color = [1.0  0   0];
            case 'g'
                color = [0.0  0.8 0];
            case 'b'
                color = [0.0  0   0.8];
            case 'c'
                color = [0.0  1  1];
            case 'm'
                color = [1.0  0  1];
            case 'y'
                color = [1.0  1  0];
            case 'k'
                color = [0.0  0  0];
            case 'w'
                color = [1.0  1  1];
            % Line shapes
            case '-'
                plotLine = true;
            % Marker styles
            case '.'
                markerShape = 0;
                plotMarkers = true;
            case 's'
                markerShape = 1;
                plotMarkers = true;
            case 'o'
                markerShape = 2;
                plotMarkers = true;
            case 'x'
                markerShape = 3;
                plotMarkers = true;
            case '+'
                markerShape = 4;
                plotMarkers = true;
            otherwise
                error('Invalid plotspec character "%s"', s)
        end
    end
end
