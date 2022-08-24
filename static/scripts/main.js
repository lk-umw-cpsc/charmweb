
var commandField;
var consoleTable;
var registers = [];
var instructions = [];

/*
JSON format:
instruction frame contains 17 instruction objects
each instruction object has two strings: address, assembly
frame is sent after a step occurs
frame data is placed in the table

register updates frame contains at least one register object
each register object has an index (0-16) and three strings: hex, integer, fp
*/

/**
 * Function called when user presses Enter within the input field
 */
function commandEntered() {
    command = commandField.value;
    if (!command || command.trim() === '') {
        return;
    }
    commandField.value = "";
    runCommand(command);
}

/**
 * Function called when the Step button is pressed
 */
function stepPressed() {
    runCommand('s');
}

function resetRegisterColors() {
    for (var register of registers) {
        register.hexDisplay.classList.remove('active');
    }
}

function callback() {
    const response = JSON.parse(this.responseText);

    resetRegisterColors();
    for (var update of response.register_updates) {
        updateRegister(update.register, update.value, 0, 0);
    }

    new_instructions = response.instruction_frame;
    for (var i = 0; i < instructions.length; i++) {
        var instruction = new_instructions[i];
        instructions[i].display.innerHTML = instruction.instruction;
        instructions[i].addressDisplay.innerHTML = instruction.address;
    }
}
/**
 * To be implemented. This will use AJAX and JSON to talk to the
 * webserver without reloading the page
 * @param {*} command 
 */
function runCommand(command) {
    // to be implemented
    // console.log(command);

    postOutput(command);
    // const contents = {message: command}
    var request = new XMLHttpRequest();
    request.open("POST", "/", true);
    request.setRequestHeader("Content-Type", "application/json");
    request.onload = callback;
    request.send(JSON.stringify({message: command}));
}

/**
 * Posts a string to the bottom of the console
 * @param {*} output The string to post
 */
function postOutput(output) {
    consoleTable.deleteRow(0);
    row = consoleTable.insertRow(-1); // -1 specifies end of table
    row.insertCell(0).innerHTML = output;
}

/**
 * Updates the instruction display with a frame of data
 * received by the server
 * @param {*} frame 
 */
function postInstructionFrame(frame) {
    for (var i = 0; i < instructions.length; i++) {
        var instruction = instructions[i];
        instruction.display.innerHTML = frame[i].assembly;
        instruction.addressDisplay.innerHTML = frame[i].address;
    }
}
/**
 * Updates a given register's display with hex, base10 and floating point values
 * @param {*} n 
 * @param {*} hex 
 * @param {*} base10 
 * @param {*} floating 
 */
function updateRegister(n, hex, base10, floating) {
    registers[n].hexDisplay.innerHTML = hex;
    registers[n].hexDisplay.classList.add('active');
}

/**
 * Function called when the page fully loads
 * This sets up important global variables and event listeners
 */
function onLoad() {
    commandField = document.getElementById("user-input");
    commandField.addEventListener("keydown", function (e) {
        if (e.key == "Enter") {
            commandEntered();
        }
    });

    consoleTable = document.getElementById("console-table");
    
    for (var i = 0; i < 17; i++) {
        var register = {};
        register.hexDisplay = document.getElementById('reg' + i + '-hex');
        registers.push(register);
    }

    for (var i = 0; i < 17; i++) {
        var instruction = {};
        instruction.addressDisplay = document.getElementById('instruction-address' + i);
        instruction.display = document.getElementById('instruction' + i);
        instructions.push(instruction);
    }

    // give console input field focus
    commandField.focus();

    // demo text
    postOutput("load os: 0");
    postOutput("os filename: no os");
    postOutput(".o filename: figisa4.0");
    postOutput("scriptfilename: no script");
    postOutput("loading figisa4.0");
    postOutput("File figisa4.o loaded!");
}

// Set up onLoad to be called once the DOM for the page is fully loaded
document.addEventListener("DOMContentLoaded", onLoad);