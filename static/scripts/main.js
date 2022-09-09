/**
 * This file contains the scripts that communicate with the
 * Flask server and display its response on the webpage.
 */
var commandField;
var consoleTable;
var consoleTableWrapper;
var registers = [];
var instructions = [];

var dumpTable = new Array(8);
var dumpPopup;

class DumpTableRow {
    constructor(row) {
        this.address = $("#dump-addr" + row)[0];
        this.values = new Array(4);
        this.values[0] = $("#dump-value" + row + "-0")[0];
        this.values[1] = $("#dump-value" + row + "-1")[0];
        this.values[2] = $("#dump-value" + row + "-2")[0];
        this.values[3] = $("#dump-value" + row + "-3")[0];
    }
}

var numConsoleDummies = 14;
var numDummiesRemoved = 0;

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
 * Function called when the "close" button is pressed within
 * the dump popup window
 */
 function hideDumpWindow() {
    dumpPopup.style.display = "none";
}

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
 * Resets the color of all the registers in the display
 * (a highlight is applied to registers changed in the previos step)
 */
function resetRegisterColors() {
    for (var register of registers) {
        register.hexDisplay.classList.remove('active');
    }
}

/**
 * Updates a given register's display with hex, base10 and floating point values
 * @param {*} n an integer value containing which register to update. cpsr
 * @param {*} hex a ten-character string with the reg's value as hexadecimal
 * @param {*} base10 TBI
 * @param {*} floating TBI
 */
function updateRegister(n, hex, base10, floating) {
    registers[n].hexDisplay.innerHTML = hex;
    registers[n].intDisplay.innerHTML = base10;
    registers[n].floatDisplay.innerHTML = floating;
    registers[n].hexDisplay.classList.add('active');
}

/**
 * Function called when we receive a JSON response from the Flask backend
 */
function responseReceived() {
    const response = JSON.parse(this.responseText);

    for (var s of response.output) {
        postOutput(s);
    }

    resetRegisterColors();
    if (response.halt) {
        $("#alternate-branch-label").hide();
        for (let i = 11; i < 16; i++) {
            instr = instructions[i];
            instr.display.innerHTML = '';
            instr.addressDisplay.innerHTML = '';
        }
        let dump = response.dump;
        if (dump.length > 0) {
            dumpPopup.style.display = "block";
            for (let i = 0; i < dump.length; i++) {
                dumpTable[i].address.innerHTML = dump[i].address;
                let values = dump[i].values;
                for (let v = 0; v < values.length; v++) {
                    dumpTable[i].values[v].innerHTML = values[v];
                }
            }
        }
    } else {
        if (response.branch) {
            $("#alternate-branch-label").show();
        } else {
            $("#alternate-branch-label").hide();
        }

        for (var update of response.register_updates) {
            updateRegister(update.register, update.value, update.int, update.float);
        }

        new_instructions = response.instruction_frame;
        for (var i = 0; i < new_instructions.length; i++) {
            var instruction = new_instructions[i];
            instructions[i].display.innerHTML = instruction.instruction;
            instructions[i].addressDisplay.innerHTML = instruction.address;
        }

        let flag_spans = document.querySelectorAll("#registers-table tr td span");
        for (let span of flag_spans) {
            span.classList.remove("active");
        }

        let flag_updates = response.flags;
        for (let i = 0; i < flag_updates.length; i++) {
            let ele = document.getElementById("flag-" + flag_updates[i].flag);
            ele.innerHTML = flag_updates[i].value;
            ele.classList.add("active");
        }
    }
}

/**
 * Function called when attempting to send the server
 * a command fails
 */
function connectionFailed() {
    postOutput("ERROR: Connection failed. Is the server up?");
}

/**
 * Sends a command to the Flask backend to be executed by the virtual machine
 * The response to this command will be handled by the callback() function
 * @param {*} command a string containing the command to execute
 */
function runCommand(command) {
    var request = new XMLHttpRequest();
    request.open("POST", "/", true);
    request.setRequestHeader("Content-Type", "application/json");
    request.onload = responseReceived;
    request.onerror = connectionFailed;
    request.send(JSON.stringify({command: command}));
}

/**
 * Posts a string to the bottom of the console
 * @param {*} output The string to post
 */
function postOutput(output) {
    if (numDummiesRemoved < numConsoleDummies) {
        consoleTable.deleteRow(0);
        numDummiesRemoved++;
    }
    var row = consoleTable.insertRow(-1); // -1 specifies end of table
    row.insertCell(0).innerHTML = output;
    row.scrollIntoView();
}

/**
 * Function called when the page fully loads
 * This sets up important global variables and event listeners (user input)
 */
function onLoad() {
    commandField = document.getElementById("user-input");
    commandField.addEventListener("keydown", function (e) {
        if (e.key == "Enter") {
            commandEntered();
        }
    });

    consoleTable = document.getElementById("console-table");
    consoleTableWrapper = document.getElementById("table-wrapper");

    // let temp = new Array(consoleTable.rows.length - 14);
    // for (var i = consoleTable.rows.length - 1; i >= 14; i--) {
    //     temp[i] = consoleTable.remove(i);
    // }

    // $("#table-wrapper")[0].style.height = $("#table-wrapper")[0].clientHeight + "px";
    // for (var i = 0; i < temp.length; i++) {
    //     consoleTable.insertRow(temp[i]);
    // }

    while (consoleTable.rows.length > numConsoleDummies && numDummiesRemoved < numConsoleDummies) {
        consoleTable.deleteRow(0);
        numDummiesRemoved++;
    }
    consoleTable.rows[consoleTable.rows.length - 1].scrollIntoView();
    // let consoleTableHeight = consoleTableWrapper.offsetHeight;
    // consoleTableWrapper.style.height = consoleTableHeight + 'px';
    
    for (var i = 0; i < 17; i++) {
        var register = {};
        register.hexDisplay = document.getElementById('reg' + i + '-hex');
        register.intDisplay = document.getElementById('reg' + i + '-int');
        register.floatDisplay = document.getElementById('reg' + i + '-float');
        registers.push(register);
    }

    for (var i = 0; i < 16; i++) {
        var instruction = {};
        instruction.addressDisplay = document.getElementById('instruction-address' + i);
        instruction.display = document.getElementById('instruction' + i);
        instructions.push(instruction);
    }

    if (instructions[11].display.innerHTML.length == 0) {
        $("#alternate-branch-label").hide();
    }

    for (var i = 0; i < dumpTable.length; i++) {
        dumpTable[i] = new DumpTableRow(i);
    }

    dumpPopup = $("#dump-window")[0];

    let rows = $("#console-table-body > tr");
    // let height = rows[0].offsetHeight;
    // $("#table-wrapper")[0].style.height = (15 * height) + "px";
    rows[rows.length - 1].scrollIntoView();
    // give console input field focus
    commandField.focus();
}

// Set up onLoad to be called once the DOM for the page is fully loaded
document.addEventListener("DOMContentLoaded", onLoad);