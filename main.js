
var commandField;
var consoleTable;
var registers = [];

/*
JSON format:
instruction frame contains 17 instruction objects
each instruction object has two strings: address, assembly
frame is placed

register updates frame contains at least one register object
each register object has an index (0-16) and three strings: hex, integer, fp
*/

function commandEntered() {
    command = commandField.value;
    if (!command || command.trim() === '') {
        return;
    }
    commandField.value = "";
    runCommand(command);
}

function stepPressed() {
    runCommand('s');
}

function runCommand(command) {
    // to be implemented
    console.log(command);
    postOutput(command);
}

function postOutput(output) {
    consoleTable.deleteRow(0);
    row = consoleTable.insertRow(-1); // -1 specifies end of table
    row.insertCell(0).innerHTML = output;
}

function postInstructionFrame() {

}

function updateRegister(n, hex, base10, floating) {
    registers[n].hexDisplay.innerHTML = hex;
}

function onLoad() {
    commandField = document.getElementById("user-input");
    commandField.addEventListener("keydown", function (e) {
        if (e.key == "Enter") {
            commandEntered();
        }
    });

    consoleTable = document.getElementById("console-table");

    postOutput("load os: 0");
    postOutput("os filename: no os");
    postOutput(".o filename: figisa4.0");
    postOutput("scriptfilename: no script");
    postOutput("loading figisa4.0");
    postOutput("File figisa4.o loaded!");
    
    for (var i = 0; i < 17; i++) {
        var register = {};
        register.hexDisplay = document.getElementById('reg' + i + '-hex');
        registers.push(register);
    }
    updateRegister(16, '0x10000000');
}

document.addEventListener("DOMContentLoaded", onLoad);