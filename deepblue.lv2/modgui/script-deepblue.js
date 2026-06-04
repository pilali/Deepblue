/* ============================================================
   DEEPBLUE modgui — value readouts
   --------------------------------------------------------------
   The knobs themselves are driven by MOD-UI core (mod-role +
   mod-widget-rotation in the icon template). This script only
   keeps the small numeric readout under each knob in sync, on
   the initial 'start' and on every 'change'.
   ============================================================ */

function (event, funcs) {

    var icon = event.icon

    function format(symbol, value) {
        if (symbol === 'wobble_rate') return Number(value).toFixed(2) + ' Hz'
        if (symbol === 'level')       return Number(value).toFixed(2) + '×'
        return Number(value).toFixed(2)
    }

    function setValue(symbol, value) {
        var els = icon.find('.db-value[data-port="' + symbol + '"]')
        if (els.length) els.text(format(symbol, value))
    }

    if (event.type === 'start') {
        var ports = event.ports || []
        for (var i = 0; i < ports.length; i++)
            setValue(ports[i].symbol, ports[i].value)
    }
    else if (event.type === 'change') {
        setValue(event.symbol, event.value)
    }
}
