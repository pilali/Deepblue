/* ============================================================
   DEEPBLUE modgui — value readouts + glowing value arcs
   --------------------------------------------------------------
   The knobs themselves are driven by MOD-UI core (mod-role +
   mod-widget-rotation in the icon template). This script keeps
   the numeric readout under each knob in sync AND fills the
   cyan value arc (the SVG overlay, stroke-dasharray on
   pathLength=100 paths), on the initial 'start' and on every
   'change' — mirroring the JUCE editor's look.
   ============================================================ */

function (event, funcs) {

    var icon = event.icon

    /* Port ranges (all others are 0–1). wobble_rate is logarithmic
       in the .ttl, so its arc fills on a log scale like the knob. */
    var RANGES = {
        wobble_rate: { min: 0.05, max: 2.0, log: true },
        level:       { min: 0.0,  max: 2.0 }
    }

    function norm(symbol, value) {
        var r = RANGES[symbol] || { min: 0.0, max: 1.0 }
        var v = r.log ? Math.log(value / r.min) / Math.log(r.max / r.min)
                      : (value - r.min) / (r.max - r.min)
        return Math.max(0, Math.min(1, v))
    }

    function format(symbol, value) {
        if (symbol === 'wobble_rate') return Number(value).toFixed(2) + ' Hz'
        if (symbol === 'level')       return Number(value).toFixed(2) + '×'
        return Number(value).toFixed(2)
    }

    function setValue(symbol, value) {
        var els = icon.find('.db-value[data-port="' + symbol + '"]')
        if (els.length) els.text(format(symbol, value))

        var arc = icon.find('.db-arc[data-arc="' + symbol + '"]')
        if (arc.length) {
            var dash = (norm(symbol, value) * 100).toFixed(2) + ' 100'
            arc.find('.db-arc-value, .db-arc-glow').attr('stroke-dasharray', dash)
        }
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
