var inputDiv;
var lwin;
var dwin;
var stylesheet;

/**
 * Function called when the page fully loads
 */
 function onLoad2() {
    inputDiv = document.getElementById("input-container");
    inputDiv.classList.add('border-highlight');
    input = document.getElementById('user-input');
    input.addEventListener('focus',  (event) => { inputDiv.classList.add('border-highlight') });
    input.addEventListener('blur', (event) => { inputDiv.classList.remove('border-highlight') });
    
    dwin = $('#dump-window')[0];
    lwin = $('#l-window')[0];
    makeDraggable(dwin, lwin);
    makeDraggable(lwin, dwin);

    let cookie = document.cookie;
    if (cookie) {
        $('#theme').attr('href', cookie);
    }

    stylesheet = $('#theme')[0];
    $('.theme-option').click(function() {
        $('#theme').attr('href', document.cookie = $(this).data('stylesheet'));
    });
}

// Set up onLoad to be called once the DOM for the page is fully loaded
document.addEventListener("DOMContentLoaded", onLoad2);

function makeDraggable(element, otherElement) {
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
        otherElement.style.zIndex = 1;
        element.style.zIndex = 2;
        previousX = e.clientX;
        previousY = e.clientY;
        document.onmousemove = doDrag;
        document.onmouseup = endDrag;
    }
    element.onmousedown = dragMouseDown;
}