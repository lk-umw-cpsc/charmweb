var inputDiv;

/**
 * Function called when the page fully loads
 */
 function onLoad2() {
    inputDiv = document.getElementById("input-container");
    inputDiv.classList.add('border-highlight');
    input = document.getElementById('user-input');
    input.addEventListener('focus',  (event) => { inputDiv.classList.add('border-highlight') });
    input.addEventListener('blur', (event) => { inputDiv.classList.remove('border-highlight') });
    makeDraggable($('#dump-window')[0]);
}

// Set up onLoad to be called once the DOM for the page is fully loaded
document.addEventListener("DOMContentLoaded", onLoad2);

function makeDraggable(element) {
    var currentX, currentY, previousX, previousY;
    function doDrag(e) {
        e = e || window.event;
        e.preventDefault();
        currentX = previousX - e.clientX;
        currentY = previousY - e.clientY;
        previousX = e.clientX;
        previousY = e.clientY;
        element.style.left = (element.offsetLeft - currentX) + 'px';
        element.style.top = (element.offsetTop - currentY) + 'px';
    }
    function endDrag(e) {
        document.onmousemove = null;
        document.onmouseup = null;
    }
    function dragMouseDown(e) {
        e = e || window.event;
        e.preventDefault();
        previousX = e.clientX;
        previousY = e.clientY;
        document.onmousemove = doDrag;
        document.onmouseup = endDrag;
    }
    element.onmousedown = dragMouseDown;
}