function dplot(position, color, markersize, markershape)
% Basic 3D plotting function for points
    global g_DisplazHold;

    if ~exist('color','var')
        color = [1 1 1];
    end
    if ~exist('markersize','var')
        markersize = 0.1;
    end
    if ~exist('markershape','var')
        markershape = 1;
    end

    if ~isempty(position)
        color = color_names(color);
        if size(position, 2) ~= 3; error('position must be a Nx3 array'); end
        if size(color, 2)    ~= 3; error('color must be a Nx3 array'); end
        nvertices = size(position, 1);
        if size(color,1) == 1
            color = repmat(color, nvertices, 1);
        end
        if size(markersize,1) == 1
            markersize = repmat(markersize, nvertices, 1);
        end
        if size(markershape,1) == 1
            markershape = repmat(markershape, nvertices, 1);
        end
        % Ensure all fields are floats for now, to avoid surprising scaling in the
        % shader
        if size(color,1) ~= nvertices; error('color must have same number of rows as position array'); end
        fileName = sprintf('%s.ply',tempname);
        fields = struct('name',{'position','color','markersize','markershape'},...
                        'semantic',{'vector_semantic','color_semantic','array_semantic','array_semantic'},...
                        'typeName',{'float64','float32','float32','uint8'},...
                        'value',{double(position),single(color),single(markersize),uint8(markershape)}...
                        );
        write_ply_points(fileName, nvertices,fields);
        if g_DisplazHold
            holdStr = '-add';
        else
            holdStr = '';
        end
        displazCall=sprintf('displaz %s -shader generic_points.glsl -rmtemp %s &',holdStr,fileName);
        system(displazCall);
        pause(0.01);
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
end

function colorVec = color_names(color)
%Convers character color labels to RGB
if ischar(color)
    switch color
        case 'r'
            colorVec = [1.0  0   0];
        case 'g'
            colorVec = [0.0  0.8 0];
        case 'b'
            colorVec = [0.0  0   0.8];
        case 'c'
            colorVec = [0.0  1  1];
        case 'm'
            colorVec = [1.0  0  1];
        case 'y'
            colorVec = [1.0  1  0];
        case 'k'
            colorVec = [0.0  0  0];
        case 'w'
            colorVec = [1.0  1  1];
        otherwise
            error('Invalid color character %s',color)
    end
else
    colorVec = color;
end
end
