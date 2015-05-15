function pos = get_dcursor_pos()

%Get the position of the point under the current displaz cursor focus

fixLdLibPath = '';
if isunix()
    % Reset LD_LIBRARY_PATH on unix - matlab adds to it to get the
    % matlab version of libraries, which tend to conflict with the
    % libraries used when building displaz.
    fixLdLibPath = 'env LD_LIBRARY_PATH=""';
end

commandLine = sprintf('%s displaz -querycursor', fixLdLibPath);

[exitStatus, commandStdout] = system(commandLine);

if exitStatus ~= 0
    errorStr = [];
    errorStr = strcat(errorStr, sprintf('ERROR: Failed to get displaz cursor position.  Stdout was:\n'));
    errorStr = strcat(errorStr, sprintf('\n%s\n',commandStdout));
    errorStr = strcat(errorStr, sprintf('\nERROR: Command Line used was:\n'));
    errorStr = strcat(errorStr, sprintf('\n%s\n',commandLine));
    error(errorStr);
end

pos = sscanf(commandStdout,'%f')';