// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Parses trace_marker events that were inserted in the trace by
 * userland.
 */
base.require('tracing.importer.linux_perf.parser');
base.exportTo('tracing.importer.linux_perf', function() {

  var Parser = tracing.importer.linux_perf.Parser;

  /**
   * Parses linux trace mark events that were inserted in the trace by userland.
   * @constructor
   */
  function ClockParser(importer) {
    Parser.call(this, importer);

    importer.registerEventHandler('clock_set_rate',
        ClockParser.prototype.traceMarkWriteClockEvent.bind(this));

    this.model_ = importer.model_;
    this.ppids_ = {};
  }

  ClockParser.prototype = {
    __proto__: Parser.prototype,

    traceMarkWriteClockEvent: function(eventName, cpuNumber, pid, ts,
                                  eventBase, threadName) {
        var event = /(\S+) state=(\d+) cpu_id=(\d+)/.exec(eventBase.details);


        var name = event[1];
        var rate = parseInt(event[2]);

        var ctr = this.model_.getOrCreateProcess(0)
              .getOrCreateCounter(null, name);
        // Initialize the counter's series fields if needed.
        if (ctr.numSeries == 0) {
            ctr.seriesNames.push('value');
            ctr.seriesColors.push(
                tracing.getStringColorId(ctr.name + '.' + 'value'));
        }

        // Add the sample value.
        ctr.timestamps.push(ts);
        ctr.samples.push(rate);

        return true;
    },
  };

  Parser.registerSubtype(ClockParser);

  return {
    ClockParser: ClockParser
  };
});
