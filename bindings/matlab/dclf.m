function dclf()
    fixLdLibPath = '';
    if isunix()
        % Reset LD_LIBRARY_PATH on unix - matlab adds to it to get the
        % matlab version of libraries, which tend to conflict with the
        % libraries used when building displaz.
        fixLdLibPath = 'env LD_LIBRARY_PATH=""';
    end
    system(sprintf('%s displaz -clear', fixLdLibPath));
end
