//-------------------------------------------------
//                      helper
//-------------------------------------------------

// Example:
//      doCreateShortcut('C:\\', 'shell_test', null, 'Desktop');
function doCreateShortcut(target, shortcutName, folder, specialFolder) {
        var shell = new ActiveXObject('WScript.Shell');

        if (!folder)
                var path = shell.SpecialFolders(specialFolder);
        else
                var path = folder;
        
        path = path + '\\' + shortcutName + '.lnk';
        
        var lnk = shell.CreateShortcut(path);
        
        lnk.TargetPath = target;
        lnk.Save();
}


//-------------------------------------------------
//                      CH 13
//-------------------------------------------------
function createShortcutInSpecialFolder(target, shortcutName, specialFolder) {
        doCreateShortcut(target, shortcutName, null, specialFolder);
}

// Example:
//      createShortcutInSpecialFolder(currentDir + '\\shell.exe', 'test', 'Desktop');
function createShortcut(target, shortcutName, folder) {
        doCreateShortcut(target, shortcutName, folder, null);
}

// If pass a key name, then read the default value of the key
// If the key end with "\\", then the function will consider
// it a key name.
// Else the function will consider it a value name
function regRead(key) {
        return new ActiveXObject('WScript.Shell').RegRead(key);
}

// If pass a key name, then write the value to the default value of the key
// If the key end with "\\", then the function will consider
// it a key name.
// Else the function will consider it a value name
function regWrite(key, value, type) {
        new ActiveXObject('WScript.Shell').RegWrite(key, value, type);
}


//-------------------------------------------------
//                      main
//-------------------------------------------------
// var shell      = new ActiveXObject('WScript.Shell');
// var currentDir = shell.CurrentDirectory;
// var regKeyRead = 'HKLM\\System\\CurrentControlSet\\Control\\ProductOptions\\ProductType';
// var regKeyWrite = 'HKLM\\System\\test';
// 
// regWrite(regKeyWrite, 12345, 'REG_SZ');
