/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

var config = config || {};

(function() {

config.kBuildNumberLimit = 20;

config.kPlatforms = {
    'chromium' : {
        label : 'Chromium',
        buildConsoleURL: 'http://build.chromium.org/p/chromium.webkit',
        layoutTestResultsURL: 'http://build.chromium.org/f/chromium/layout_test_results',
        waterfallURL: 'http://build.chromium.org/p/chromium.webkit/waterfall',
        builders: {
            'WebKit XP': {version: 'xp'},
            'WebKit Win7': {version: 'win7'},
            'WebKit Win7 (dbg)(1)': {version: 'win7', debug: true},
            'WebKit Win7 (dbg)(2)': {version: 'win7', debug: true},
            'WebKit Linux': {version: 'lucid', is64bit: true},
            'WebKit Linux 32': {version: 'lucid'},
            'WebKit Linux (dbg)': {version: 'lucid', is64bit: true, debug: true},
            'WebKit Mac10.6': {version: 'snowleopard'},
            'WebKit Mac10.6 (dbg)': {version: 'snowleopard', debug: true},
            'WebKit Mac10.7': {version: 'lion'},
            'WebKit Mac10.7 (dbg)': {version: 'lion', debug: true},
            'WebKit Mac10.8': {version: 'mountainlion'},
        },
        haveBuilderAccumulatedResults : true,
        useDirectoryListingForOldBuilds: true,
        useFlakinessDashboard: true,
        resultsDirectoryNameFromBuilderName: function(builderName) {
            return base.underscoredBuilderName(builderName);
        },
        resultsDirectoryForBuildNumber: function(buildNumber, revision) {
            return buildNumber;
        },
        _builderApplies: function(builderName) {
            // FIXME: Should garden-o-matic show these?
            // WebKit Android and ASAN are red all the time.
            // Remove this function entirely once they are better supported.
            return builderName.indexOf('GPU') == -1 &&
                   builderName.indexOf('ASAN') == -1 &&
                   builderName.indexOf('WebKit (Content Shell) Android') == -1 &&
                   builderName.indexOf('WebKit Android') == -1;
        },
    },
};

config.currentPlatform = 'chromium';
config.kBlinkSvnURL = 'svn://svn.chromium.org/blink/trunk';
config.kBlinkRevisionURL = 'http://src.chromium.org/viewvc/blink';
config.kSvnLogURL = 'http://build.chromium.org/cgi-bin/svn-log';
config.kNumberOfRecentCommits = 50;

config.kBugzillaURL = 'https://bugs.webkit.org';

config.kRevisionAttr = 'data-revision';
config.kTestNameAttr = 'data-test-name';
config.kBuilderNameAttr = 'data-builder-name';
config.kFailureCountAttr = 'data-failure-count';
config.kFailureTypesAttr = 'data-failure-types';
config.kInfobarTypeAttr = 'data-infobar-type';

var kTenMinutesInMilliseconds = 10 * 60 * 1000;
config.kUpdateFrequency = kTenMinutesInMilliseconds;
config.kRelativeTimeUpdateFrequency = 1000 * 60;

config.kExperimentalFeatures = window.location.search.search('enableExperiments=1') != -1;

config.currentPlatform = base.getURLParameter('platform') || 'chromium';

// FIXME: We should add a way to restrict the results to a subset of the builders
// (or maybe just a single builder) in the UI as well as via an URL parameter.
config.currentBuilder = base.getURLParameter('builder');

config.currentBuilders = function() {
    var current_builders = {};
    if (config.currentBuilder) {
        current_builders[config.currentBuilder] = config.kPlatforms[config.currentPlatform].builders[config.currentBuilder];
        return current_builders;
    } else {
        return config.kPlatforms[config.currentPlatform].builders;
    }
};

config.builderApplies = function(builderName) {
    if (config.currentBuilder)
        return builderName == config.currentBuilder;
    return config.kPlatforms[config.currentPlatform]._builderApplies(builderName);
};

config.useLocalResults = Boolean(base.getURLParameter('useLocalResults')) || false;

})();
