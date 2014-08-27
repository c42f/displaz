function dhold(h)
    global g_DisplazHold;

    if nargin == 1 && ( isnumeric(h) || islogical(h) )
        g_DisplazHold = logical(h);
    elseif nargin == 1 && ischar(h)
        if strcmp(h,'on')
            g_DisplazHold = true;
        elseif strcmp(h,'off')
            g_DisplazHold = false;
        else
            warning('Invalid hold option %s',h);
        end
    elseif ~isempty(g_DisplazHold)
        g_DisplazHold = ~g_DisplazHold;
    else
        g_DisplazHold = true;
    end
end
