/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var expect = chai.expect;

describe("loop.conversationViews", function () {
  "use strict";

  var sharedUtils = loop.shared.utils;
  var sharedView = loop.shared.views;
  var sandbox, view, dispatcher, contact, fakeAudioXHR;
  var fakeMozLoop, fakeWindow;

  var CALL_STATES = loop.store.CALL_STATES;
  var CALL_TYPES = loop.shared.utils.CALL_TYPES;
  var REST_ERRNOS = loop.shared.utils.REST_ERRNOS;
  var WEBSOCKET_REASONS = loop.shared.utils.WEBSOCKET_REASONS;

  // XXX refactor to Just Work with "sandbox.stubComponent" or else
  // just pass in the sandbox and put somewhere generally usable

  function stubComponent(obj, component, mockTagName){
    var reactClass = React.createClass({
      render: function() {
        var mockTagName = mockTagName || "div";
        return React.DOM[mockTagName](null, this.props.children);
      }
    });
    return sandbox.stub(obj, component, reactClass);
  }

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    sandbox.useFakeTimers();

    sandbox.stub(document.mozL10n, "get", function(x) {
      return x;
    });

    dispatcher = new loop.Dispatcher();
    sandbox.stub(dispatcher, "dispatch");

    contact = {
      name: [ "mrsmith" ],
      email: [{
        type: "home",
        value: "fakeEmail",
        pref: true
      }]
    };
    fakeAudioXHR = {
      open: sinon.spy(),
      send: function() {},
      abort: function() {},
      getResponseHeader: function(header) {
        if (header === "Content-Type")
          return "audio/ogg";
      },
      responseType: null,
      response: new ArrayBuffer(10),
      onload: null
    };

    fakeMozLoop = navigator.mozLoop = {
      // Dummy function, stubbed below.
      getLoopPref: function() {},
      calls: {
        clearCallInProgress: sinon.stub()
      },
      composeEmail: sinon.spy(),
      get appVersionInfo() {
        return {
          version: "42",
          channel: "test",
          platform: "test"
        };
      },
      getAudioBlob: sinon.spy(function(name, callback) {
        callback(null, new Blob([new ArrayBuffer(10)], {type: "audio/ogg"}));
      }),
      startAlerting: sinon.stub(),
      stopAlerting: sinon.stub(),
      userProfile: {
        email: "bob@invalid.tld"
      }
    };
    sinon.stub(fakeMozLoop, "getLoopPref", function(pref) {
        if (pref === "fake") {
          return"http://fakeurl";
        }

        return false;
    });

    fakeWindow = {
      navigator: { mozLoop: fakeMozLoop },
      close: sinon.stub(),
      document: {},
      addEventListener: function() {},
      removeEventListener: function() {}
    };
    loop.shared.mixins.setRootObject(fakeWindow);

  });

  afterEach(function() {
    loop.shared.mixins.setRootObject(window);
    view = undefined;
    delete navigator.mozLoop;
    sandbox.restore();
  });

  describe("CallIdentifierView", function() {
    function mountTestComponent(props) {
      return TestUtils.renderIntoDocument(
        React.createElement(loop.conversationViews.CallIdentifierView, props));
    }

    it("should set display the peer identifer", function() {
      view = mountTestComponent({
        showIcons: false,
        peerIdentifier: "mrssmith"
      });

      expect(TestUtils.findRenderedDOMComponentWithClass(
        view, "fx-embedded-call-identifier-text").props.children).eql("mrssmith");
    });

    it("should not display the icons if showIcons is false", function() {
      view = mountTestComponent({
        showIcons: false,
        peerIdentifier: "mrssmith"
      });

      expect(TestUtils.findRenderedDOMComponentWithClass(
        view, "fx-embedded-call-detail").props.className).to.contain("hide");
    });

    it("should display the icons if showIcons is true", function() {
      view = mountTestComponent({
        showIcons: true,
        peerIdentifier: "mrssmith"
      });

      expect(TestUtils.findRenderedDOMComponentWithClass(
        view, "fx-embedded-call-detail").props.className).to.not.contain("hide");
    });

    it("should display the url timestamp", function() {
      sandbox.stub(loop.shared.utils, "formatDate").returns(("October 9, 2014"));

      view = mountTestComponent({
        showIcons: true,
        peerIdentifier: "mrssmith",
        urlCreationDate: (new Date() / 1000).toString()
      });

      expect(TestUtils.findRenderedDOMComponentWithClass(
        view, "fx-embedded-conversation-timestamp").props.children).eql("(October 9, 2014)");
    });

    it("should show video as muted if video is false", function() {
      view = mountTestComponent({
        showIcons: true,
        peerIdentifier: "mrssmith",
        video: false
      });

      expect(TestUtils.findRenderedDOMComponentWithClass(
        view, "fx-embedded-tiny-video-icon").props.className).to.contain("muted");
    });
  });

  describe("PendingConversationView", function() {
    function mountTestComponent(props) {
      return TestUtils.renderIntoDocument(
        React.createElement(loop.conversationViews.PendingConversationView, props));
    }

    it("should set display connecting string when the state is not alerting",
      function() {
        view = mountTestComponent({
          callState: CALL_STATES.CONNECTING,
          contact: contact,
          dispatcher: dispatcher
        });

        var label = TestUtils.findRenderedDOMComponentWithClass(
          view, "btn-label").props.children;

        expect(label).to.have.string("connecting");
    });

    it("should set display ringing string when the state is alerting",
      function() {
        view = mountTestComponent({
          callState: CALL_STATES.ALERTING,
          contact: contact,
          dispatcher: dispatcher
        });

        var label = TestUtils.findRenderedDOMComponentWithClass(
          view, "btn-label").props.children;

        expect(label).to.have.string("ringing");
    });

    it("should disable the cancel button if enableCancelButton is false",
      function() {
        view = mountTestComponent({
          callState: CALL_STATES.CONNECTING,
          contact: contact,
          dispatcher: dispatcher,
          enableCancelButton: false
        });

        var cancelBtn = view.getDOMNode().querySelector('.btn-cancel');

        expect(cancelBtn.classList.contains("disabled")).eql(true);
      });

    it("should enable the cancel button if enableCancelButton is false",
      function() {
        view = mountTestComponent({
          callState: CALL_STATES.CONNECTING,
          contact: contact,
          dispatcher: dispatcher,
          enableCancelButton: true
        });

        var cancelBtn = view.getDOMNode().querySelector('.btn-cancel');

        expect(cancelBtn.classList.contains("disabled")).eql(false);
      });

    it("should dispatch a cancelCall action when the cancel button is pressed",
      function() {
        view = mountTestComponent({
          callState: CALL_STATES.CONNECTING,
          contact: contact,
          dispatcher: dispatcher
        });

        var cancelBtn = view.getDOMNode().querySelector('.btn-cancel');

        React.addons.TestUtils.Simulate.click(cancelBtn);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "cancelCall"));
      });
  });

  describe("CallFailedView", function() {
    var store, fakeAudio;

    var contact = {email: [{value: "test@test.tld"}]};

    function mountTestComponent(options) {
      options = options || {};
      return TestUtils.renderIntoDocument(
        React.createElement(loop.conversationViews.CallFailedView, {
          dispatcher: dispatcher,
          contact: options.contact
        }));
    }

    beforeEach(function() {
      store = new loop.store.ConversationStore(dispatcher, {
        client: {},
        mozLoop: navigator.mozLoop,
        sdkDriver: {}
      });
      loop.store.StoreMixin.register({
        conversationStore: store
      });

      fakeAudio = {
        play: sinon.spy(),
        pause: sinon.spy(),
        removeAttribute: sinon.spy()
      };
      sandbox.stub(window, "Audio").returns(fakeAudio);
    });

    it("should dispatch a retryCall action when the retry button is pressed",
      function() {
        view = mountTestComponent({contact: contact});

        var retryBtn = view.getDOMNode().querySelector('.btn-retry');

        React.addons.TestUtils.Simulate.click(retryBtn);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "retryCall"));
      });

    it("should dispatch a cancelCall action when the cancel button is pressed",
      function() {
        view = mountTestComponent({contact: contact});

        var cancelBtn = view.getDOMNode().querySelector('.btn-cancel');

        React.addons.TestUtils.Simulate.click(cancelBtn);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "cancelCall"));
      });

    it("should dispatch a fetchRoomEmailLink action when the email button is pressed",
      function() {
        view = mountTestComponent({contact: contact});

        var emailLinkBtn = view.getDOMNode().querySelector('.btn-email');

        React.addons.TestUtils.Simulate.click(emailLinkBtn);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "fetchRoomEmailLink"));
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("roomOwner", fakeMozLoop.userProfile.email));
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("roomName", "test@test.tld"));
      });

    it("should name the created room using the contact name when available",
      function() {
        view = mountTestComponent({contact: {
          email: [{value: "test@test.tld"}],
          name: ["Mr Fake ContactName"]
        }});

        var emailLinkBtn = view.getDOMNode().querySelector('.btn-email');

        React.addons.TestUtils.Simulate.click(emailLinkBtn);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("roomName", "Mr Fake ContactName"));
      });

    it("should disable the email link button once the action is dispatched",
      function() {
        view = mountTestComponent({contact: contact});
        var emailLinkBtn = view.getDOMNode().querySelector('.btn-email');
        React.addons.TestUtils.Simulate.click(emailLinkBtn);

        expect(view.getDOMNode().querySelector(".btn-email").disabled).eql(true);
      });

    it("should compose an email once the email link is received", function() {
      var composeCallUrlEmail = sandbox.stub(sharedUtils, "composeCallUrlEmail");
      view = mountTestComponent({contact: contact});
      store.setStoreState({emailLink: "http://fake.invalid/"});

      sinon.assert.calledOnce(composeCallUrlEmail);
      sinon.assert.calledWithExactly(composeCallUrlEmail,
        "http://fake.invalid/", "test@test.tld");
    });

    it("should close the conversation window once the email link is received",
      function() {
        view = mountTestComponent({contact: contact});

        store.setStoreState({emailLink: "http://fake.invalid/"});

        sinon.assert.calledOnce(fakeWindow.close);
      });

    it("should display an error message in case email link retrieval failed",
      function() {
        view = mountTestComponent({contact: contact});

        store.trigger("error:emailLink");

        expect(view.getDOMNode().querySelector(".error")).not.eql(null);
      });

    it("should allow retrying to get a call url if it failed previously",
      function() {
        view = mountTestComponent({contact: contact});

        store.trigger("error:emailLink");

        expect(view.getDOMNode().querySelector(".btn-email").disabled).eql(false);
      });

    it("should play a failure sound, once", function() {
      view = mountTestComponent({contact: contact});

      sinon.assert.calledOnce(navigator.mozLoop.getAudioBlob);
      sinon.assert.calledWithExactly(navigator.mozLoop.getAudioBlob,
                                     "failure", sinon.match.func);
      sinon.assert.calledOnce(fakeAudio.play);
      expect(fakeAudio.loop).to.equal(false);
    });

    it("should show 'something went wrong' when the reason is WEBSOCKET_REASONS.MEDIA_FAIL",
      function () {
        store.setStoreState({callStateReason: WEBSOCKET_REASONS.MEDIA_FAIL});

        view = mountTestComponent({contact: contact});

        sinon.assert.calledWith(document.mozL10n.get, "generic_failure_title");
      });

    it("should show 'contact unavailable' when the reason is WEBSOCKET_REASONS.REJECT",
      function () {
        store.setStoreState({callStateReason: WEBSOCKET_REASONS.REJECT});

        view = mountTestComponent({contact: contact});

        sinon.assert.calledWithExactly(document.mozL10n.get,
          "contact_unavailable_title",
          {contactName: loop.conversationViews._getContactDisplayName(contact)});
      });

    it("should show 'contact unavailable' when the reason is WEBSOCKET_REASONS.BUSY",
      function () {
        store.setStoreState({callStateReason: WEBSOCKET_REASONS.BUSY});

        view = mountTestComponent({contact: contact});

        sinon.assert.calledWithExactly(document.mozL10n.get,
          "contact_unavailable_title",
          {contactName: loop.conversationViews._getContactDisplayName(contact)});
      });

    it("should show 'something went wrong' when the reason is 'setup'",
      function () {
        store.setStoreState({callStateReason: "setup"});

        view = mountTestComponent({contact: contact});

        sinon.assert.calledWithExactly(document.mozL10n.get,
          "generic_failure_title");
      });

    it("should show 'contact unavailable' when the reason is REST_ERRNOS.USER_UNAVAILABLE",
      function () {
        store.setStoreState({callStateReason: REST_ERRNOS.USER_UNAVAILABLE});

        view = mountTestComponent({contact: contact});

        sinon.assert.calledWithExactly(document.mozL10n.get,
          "contact_unavailable_title",
          {contactName: loop.conversationViews._getContactDisplayName(contact)});
      });

    it("should display a generic contact unavailable msg when the reason is" +
       " WEBSOCKET_REASONS.BUSY and no display name is available", function() {
        store.setStoreState({callStateReason: WEBSOCKET_REASONS.BUSY});
        var phoneOnlyContact = {
          tel: [{"pref": true, type: "work", value: ""}]
        };

        view = mountTestComponent({contact: phoneOnlyContact});

        sinon.assert.calledWith(document.mozL10n.get,
          "generic_contact_unavailable_title");
    });
  });

  describe("OngoingConversationView", function() {
    function mountTestComponent(props) {
      return TestUtils.renderIntoDocument(
        React.createElement(loop.conversationViews.OngoingConversationView, props));
    }

    it("should dispatch a setupStreamElements action when the view is created",
      function() {
        view = mountTestComponent({
          dispatcher: dispatcher
        });

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "setupStreamElements"));
      });

    it("should dispatch a hangupCall action when the hangup button is pressed",
      function() {
        view = mountTestComponent({
          dispatcher: dispatcher
        });

        var hangupBtn = view.getDOMNode().querySelector('.btn-hangup');

        React.addons.TestUtils.Simulate.click(hangupBtn);

        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "hangupCall"));
      });

    it("should dispatch a setMute action when the audio mute button is pressed",
      function() {
        view = mountTestComponent({
          dispatcher: dispatcher,
          audio: {enabled: false}
        });

        var muteBtn = view.getDOMNode().querySelector('.btn-mute-audio');

        React.addons.TestUtils.Simulate.click(muteBtn);

        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "setMute"));
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("enabled", true));
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("type", "audio"));
      });

    it("should dispatch a setMute action when the video mute button is pressed",
      function() {
        view = mountTestComponent({
          dispatcher: dispatcher,
          video: {enabled: true}
        });

        var muteBtn = view.getDOMNode().querySelector('.btn-mute-video');

        React.addons.TestUtils.Simulate.click(muteBtn);

        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "setMute"));
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("enabled", false));
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("type", "video"));
      });

    it("should set the mute button as mute off", function() {
      view = mountTestComponent({
        dispatcher: dispatcher,
        video: {enabled: true}
      });

      var muteBtn = view.getDOMNode().querySelector('.btn-mute-video');

      expect(muteBtn.classList.contains("muted")).eql(false);
    });

    it("should set the mute button as mute on", function() {
      view = mountTestComponent({
        dispatcher: dispatcher,
        audio: {enabled: false}
      });

      var muteBtn = view.getDOMNode().querySelector('.btn-mute-audio');

      expect(muteBtn.classList.contains("muted")).eql(true);
    });
  });

  describe("CallControllerView", function() {
    var store, feedbackStore;

    function mountTestComponent() {
      return TestUtils.renderIntoDocument(
        React.createElement(loop.conversationViews.CallControllerView, {
          dispatcher: dispatcher,
          mozLoop: fakeMozLoop
        }));
    }

    beforeEach(function() {
      store = new loop.store.ConversationStore(dispatcher, {
        client: {},
        mozLoop: fakeMozLoop,
        sdkDriver: {}
      });
      loop.store.StoreMixin.register({
        conversationStore: store
      });

      feedbackStore = new loop.store.FeedbackStore(dispatcher, {
        feedbackClient: {}
      });
    });

    it("should set the document title to the callerId", function() {
      store.setStoreState({
        contact: contact
      });

      mountTestComponent();

      expect(fakeWindow.document.title).eql("mrsmith");
    });

    it("should fallback to the contact email if the contact name is not defined", function() {
      delete contact.name;
      store.setStoreState({
        contact: contact
      });

      mountTestComponent({contact: contact});

      expect(fakeWindow.document.title).eql("fakeEmail");
    });

    it("should fallback to the caller id if no contact is defined", function() {
      store.setStoreState({
        callerId: "fakeId"
      });

      mountTestComponent({contact: contact});

      expect(fakeWindow.document.title).eql("fakeId");
    });

    it("should render the CallFailedView when the call state is 'terminated'",
      function() {
        store.setStoreState({
          callState: CALL_STATES.TERMINATED,
          contact: contact
        });

        view = mountTestComponent();

        TestUtils.findRenderedComponentWithType(view,
          loop.conversationViews.CallFailedView);
    });

    it("should render the PendingConversationView for outgoing calls when the call state is 'gather'",
      function() {
        store.setStoreState({
          callState: CALL_STATES.GATHER,
          contact: contact,
          outgoing: true
        });

        view = mountTestComponent();

        TestUtils.findRenderedComponentWithType(view,
          loop.conversationViews.PendingConversationView);
    });

    it("should render the AcceptCallView for incoming calls when the call state is 'alerting'", function() {
      store.setStoreState({
        callState: CALL_STATES.ALERTING,
        outgoing: false
      });

      view = mountTestComponent();

      TestUtils.findRenderedComponentWithType(view,
        loop.conversationViews.AcceptCallView);
    });

    it("should render the OngoingConversationView when the call state is 'ongoing'",
      function() {
        store.setStoreState({callState: CALL_STATES.ONGOING});

        view = mountTestComponent();

        TestUtils.findRenderedComponentWithType(view,
          loop.conversationViews.OngoingConversationView);
    });

    it("should render the FeedbackView when the call state is 'finished'",
      function() {
        store.setStoreState({callState: CALL_STATES.FINISHED});

        view = mountTestComponent();

        TestUtils.findRenderedComponentWithType(view,
          loop.shared.views.FeedbackView);
    });

    it("should set the document title to conversation_has_ended when displaying the feedback view", function() {
      store.setStoreState({callState: CALL_STATES.FINISHED});

      mountTestComponent();

      expect(fakeWindow.document.title).eql("conversation_has_ended");
    });

    it("should play the terminated sound when the call state is 'finished'",
      function() {
        var fakeAudio = {
          play: sinon.spy(),
          pause: sinon.spy(),
          removeAttribute: sinon.spy()
        };
        sandbox.stub(window, "Audio").returns(fakeAudio);

        store.setStoreState({callState: CALL_STATES.FINISHED});

        view = mountTestComponent();

        sinon.assert.calledOnce(fakeAudio.play);
    });

    it("should update the rendered views when the state is changed.",
      function() {
        store.setStoreState({
          callState: CALL_STATES.GATHER,
          contact: contact,
          outgoing: true
        });

        view = mountTestComponent();

        TestUtils.findRenderedComponentWithType(view,
          loop.conversationViews.PendingConversationView);

        store.setStoreState({callState: CALL_STATES.TERMINATED});

        TestUtils.findRenderedComponentWithType(view,
          loop.conversationViews.CallFailedView);
    });
  });

  describe("AcceptCallView", function() {
    var view;

    function mountTestComponent(extraProps) {
      var props = _.extend({dispatcher: dispatcher, mozLoop: fakeMozLoop}, extraProps);
      return TestUtils.renderIntoDocument(
        React.createElement(loop.conversationViews.AcceptCallView, props));
    }

    afterEach(function() {
      view = null;
    });

    it("should start alerting on display", function() {
      view = mountTestComponent({
        callType: CALL_TYPES.AUDIO_VIDEO,
        callerId: "fake@invalid.com"
      });

      sinon.assert.calledOnce(fakeMozLoop.startAlerting);
    });

    it("should stop alerting when removed from the display", function() {
      view = mountTestComponent({
        callType: CALL_TYPES.AUDIO_VIDEO,
        callerId: "fake@invalid.com"
      });

      view.componentWillUnmount();

      sinon.assert.calledOnce(fakeMozLoop.stopAlerting);
    });

    describe("default answer mode", function() {
      it("should display video as primary answer mode", function() {
        view = mountTestComponent({
          callType: CALL_TYPES.AUDIO_VIDEO,
          callerId: "fake@invalid.com"
        });

        var primaryBtn = view.getDOMNode()
                                  .querySelector('.fx-embedded-btn-icon-video');

        expect(primaryBtn).not.to.eql(null);
      });

      it("should display audio as primary answer mode", function() {
        view = mountTestComponent({
          callType: CALL_TYPES.AUDIO_ONLY,
          callerId: "fake@invalid.com"
        });

        var primaryBtn = view.getDOMNode()
                                  .querySelector('.fx-embedded-btn-icon-audio');

        expect(primaryBtn).not.to.eql(null);
      });

      it("should accept call with video", function() {
        view = mountTestComponent({
          callType: CALL_TYPES.AUDIO_VIDEO,
          callerId: "fake@invalid.com"
        });

        var primaryBtn = view.getDOMNode()
                                  .querySelector('.fx-embedded-btn-icon-video');

        React.addons.TestUtils.Simulate.click(primaryBtn);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.AcceptCall({
            callType: CALL_TYPES.AUDIO_VIDEO
          }));
      });

      it("should accept call with audio", function() {
        view = mountTestComponent({
          callType: CALL_TYPES.AUDIO_ONLY,
          callerId: "fake@invalid.com"
        });

        var primaryBtn = view.getDOMNode()
                                  .querySelector('.fx-embedded-btn-icon-audio');

        React.addons.TestUtils.Simulate.click(primaryBtn);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.AcceptCall({
            callType: CALL_TYPES.AUDIO_ONLY
          }));
      });

      it("should accept call with video when clicking on secondary btn",
        function() {
          view = mountTestComponent({
            callType: CALL_TYPES.AUDIO_ONLY,
            callerId: "fake@invalid.com"
          });

          var secondaryBtn = view.getDOMNode()
          .querySelector('.fx-embedded-btn-video-small');

          React.addons.TestUtils.Simulate.click(secondaryBtn);

          sinon.assert.calledOnce(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.AcceptCall({
              callType: CALL_TYPES.AUDIO_VIDEO
            }));
        });

      it("should accept call with audio when clicking on secondary btn",
        function() {
          view = mountTestComponent({
            callType: CALL_TYPES.AUDIO_VIDEO,
            callerId: "fake@invalid.com"
          });

          var secondaryBtn = view.getDOMNode()
          .querySelector('.fx-embedded-btn-audio-small');

          React.addons.TestUtils.Simulate.click(secondaryBtn);

          sinon.assert.calledOnce(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.AcceptCall({
              callType: CALL_TYPES.AUDIO_ONLY
            }));
        });
    });

    describe("click event on .btn-decline", function() {
      it("should dispatch a DeclineCall action", function() {
        view = mountTestComponent({
          callType: CALL_TYPES.AUDIO_VIDEO,
          callerId: "fake@invalid.com"
        });

        var buttonDecline = view.getDOMNode().querySelector(".btn-decline");

        TestUtils.Simulate.click(buttonDecline);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.DeclineCall({blockCaller: false}));
      });
    });

    describe("click event on .btn-block", function() {
      it("should dispatch a DeclineCall action with blockCaller true", function() {
        view = mountTestComponent({
          callType: CALL_TYPES.AUDIO_VIDEO,
          callerId: "fake@invalid.com"
        });

        var buttonBlock = view.getDOMNode().querySelector(".btn-block");

        TestUtils.Simulate.click(buttonBlock);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.DeclineCall({blockCaller: true}));
      });
    });
  });

  describe("GenericFailureView", function() {
    var view, fakeAudio;

    beforeEach(function() {
      fakeAudio = {
        play: sinon.spy(),
        pause: sinon.spy(),
        removeAttribute: sinon.spy()
      };
      navigator.mozLoop.doNotDisturb = false;
      sandbox.stub(window, "Audio").returns(fakeAudio);

      view = TestUtils.renderIntoDocument(
        React.createElement(loop.conversationViews.GenericFailureView, {
          cancelCall: function() {}
        }));
    });

    it("should play a failure sound, once", function() {
      sinon.assert.calledOnce(navigator.mozLoop.getAudioBlob);
      sinon.assert.calledWithExactly(navigator.mozLoop.getAudioBlob,
                                     "failure", sinon.match.func);
      sinon.assert.calledOnce(fakeAudio.play);
      expect(fakeAudio.loop).to.equal(false);
    });

    it("should set the title to generic_failure_title", function() {
      expect(fakeWindow.document.title).eql("generic_failure_title");
    });
  });
});
