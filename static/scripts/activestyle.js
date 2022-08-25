var inputDiv;

/**
 * Function called when the page fully loads
 */
 function onLoad2() {
    inputDiv = document.getElementById("input-container");
    input = document.getElementById('user-input');
    input.addEventListener('focus',  (event) => { inputDiv.classList.add('border-highlight') });
    input.addEventListener('blur', (event) => { inputDiv.classList.remove('border-highlight') });
}

// Set up onLoad to be called once the DOM for the page is fully loaded
document.addEventListener("DOMContentLoaded", onLoad2);