/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.EditBookmarkDialog;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.R;
import org.mozilla.gecko.ReaderModeUtils;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.favicons.Favicons;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UiAsyncTask;

import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.util.Log;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.Toast;

/**
 * HomeFragment is an empty fragment that can be added to the HomePager.
 * Subclasses can add their own views. 
 */
abstract class HomeFragment extends Fragment {
    // Log Tag.
    private static final String LOGTAG="GeckoHomeFragment";

    // Share MIME type.
    protected static final String SHARE_MIME_TYPE = "text/plain";

    // Default value for "can load" hint
    static final boolean DEFAULT_CAN_LOAD_HINT = false;

    // Whether the fragment can load its content or not
    // This is used to defer data loading until the editing
    // mode animation ends.
    private boolean mCanLoadHint;

    // Whether the fragment has loaded its content
    private boolean mIsLoaded;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        final Bundle args = getArguments();
        if (args != null) {
            mCanLoadHint = args.getBoolean(HomePager.CAN_LOAD_ARG, DEFAULT_CAN_LOAD_HINT);
        } else {
            mCanLoadHint = DEFAULT_CAN_LOAD_HINT;
        }

        mIsLoaded = false;
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View view, ContextMenuInfo menuInfo) {
        if (menuInfo == null || !(menuInfo instanceof HomeContextMenuInfo)) {
            return;
        }

        HomeContextMenuInfo info = (HomeContextMenuInfo) menuInfo;

        // Don't show the context menu for folders.
        if (info.isFolder) {
            return;
        }

        MenuInflater inflater = new MenuInflater(view.getContext());
        inflater.inflate(R.menu.home_contextmenu, menu);

        menu.setHeaderTitle(info.getDisplayTitle());

        // Hide ununsed menu items.
        menu.findItem(R.id.top_sites_edit).setVisible(false);
        menu.findItem(R.id.top_sites_pin).setVisible(false);
        menu.findItem(R.id.top_sites_unpin).setVisible(false);

        // Hide the "Edit" menuitem if this item isn't a bookmark,
        // or if this is a reading list item.
        if (!info.hasBookmarkId() || info.isInReadingList()) {
            menu.findItem(R.id.home_edit_bookmark).setVisible(false);
        }

        // Hide the "Remove" menuitem if this item not removable.
        if (!info.canRemove()) {
            menu.findItem(R.id.home_remove).setVisible(false);
        }

        menu.findItem(R.id.home_share).setVisible(!GeckoProfile.get(getActivity()).inGuestMode());

        final boolean canOpenInReader = (info.display == Combined.DISPLAY_READER);
        menu.findItem(R.id.home_open_in_reader).setVisible(canOpenInReader);
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        // onContextItemSelected() is first dispatched to the activity and
        // then dispatched to its fragments. Since fragments cannot "override"
        // menu item selection handling, it's better to avoid menu id collisions
        // between the activity and its fragments.

        ContextMenuInfo menuInfo = item.getMenuInfo();
        if (menuInfo == null || !(menuInfo instanceof HomeContextMenuInfo)) {
            return false;
        }

        final HomeContextMenuInfo info = (HomeContextMenuInfo) menuInfo;
        final Context context = getActivity();

        final int itemId = item.getItemId();
        if (itemId == R.id.home_share) {
            if (info.url == null) {
                Log.e(LOGTAG, "Can't share because URL is null");
                return false;
            } else {
                GeckoAppShell.openUriExternal(info.url, SHARE_MIME_TYPE, "", "",
                                              Intent.ACTION_SEND, info.getDisplayTitle());

                // Context: Sharing via chrome homepage contextmenu list (home session should be active)
                Telemetry.sendUIEvent(TelemetryContract.Event.SHARE, TelemetryContract.Method.LIST);
                return true;
            }
        }

        if (itemId == R.id.home_add_to_launcher) {
            if (info.url == null) {
                Log.e(LOGTAG, "Can't add to home screen because URL is null");
                return false;
            }

            // Fetch an icon big enough for use as a home screen icon.
            Favicons.getPreferredSizeFaviconForPage(info.url, new GeckoAppShell.CreateShortcutFaviconLoadedListener(info.url, info.getDisplayTitle()));
            return true;
        }

        if (itemId == R.id.home_open_private_tab || itemId == R.id.home_open_new_tab) {
            if (info.url == null) {
                Log.e(LOGTAG, "Can't open in new tab because URL is null");
                return false;
            }

            int flags = Tabs.LOADURL_NEW_TAB | Tabs.LOADURL_BACKGROUND;
            if (item.getItemId() == R.id.home_open_private_tab)
                flags |= Tabs.LOADURL_PRIVATE;

            Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.CONTEXT_MENU);

            final String url = (info.isInReadingList() ? ReaderModeUtils.getAboutReaderForUrl(info.url) : info.url);

            // Some pinned site items have "user-entered" urls. URLs entered in the PinSiteDialog are wrapped in
            // a special URI until we can get a valid URL. If the url is a user-entered url, decode the URL before loading it.
            Tabs.getInstance().loadUrl(decodeUserEnteredUrl(url), flags);
            Toast.makeText(context, R.string.new_tab_opened, Toast.LENGTH_SHORT).show();
            return true;
        }

        if (itemId == R.id.home_edit_bookmark) {
            // UI Dialog associates to the activity context, not the applications'.
            new EditBookmarkDialog(context).show(info.url);
            return true;
        }

        if (itemId == R.id.home_open_in_reader) {
            final String url = ReaderModeUtils.getAboutReaderForUrl(info.url);
            Tabs.getInstance().loadUrl(url, Tabs.LOADURL_NONE);
            return true;
        }

        if (itemId == R.id.home_remove) {
            // Prioritize removing a history entry over a bookmark in the case of a combined item.
            if (info.hasHistoryId()) {
                new RemoveHistoryTask(context, info.historyId).execute();
                return true;
            }

            if (info.hasBookmarkId()) {
                new RemoveBookmarkTask(context, info.bookmarkId).execute();
                return true;
            }

            if (info.isInReadingList()) {
                (new RemoveReadingListItemTask(context, info.readingListItemId, info.url)).execute();
                return true;
            }
        }

        return false;
    }

    @Override
    public void setUserVisibleHint (boolean isVisibleToUser) {
        if (isVisibleToUser == getUserVisibleHint()) {
            return;
        }

        super.setUserVisibleHint(isVisibleToUser);
        loadIfVisible();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
    }

    void setCanLoadHint(boolean canLoadHint) {
        if (mCanLoadHint == canLoadHint) {
            return;
        }

        mCanLoadHint = canLoadHint;
        loadIfVisible();
    }

    boolean getCanLoadHint() {
        return mCanLoadHint;
    }

    /**
     * Given a url with a user-entered scheme, extract the
     * scheme-specific component. For e.g, given "user-entered://www.google.com",
     * this method returns "//www.google.com". If the passed url
     * does not have a user-entered scheme, the same url will be returned.
     *
     * @param  url to be decoded
     * @return url component entered by user
     */
    public static String decodeUserEnteredUrl(String url) {
        Uri uri = Uri.parse(url);
        if ("user-entered".equals(uri.getScheme())) {
            return uri.getSchemeSpecificPart();
        }
        return url;
    }

    protected abstract void load();

    protected boolean canLoad() {
        return (mCanLoadHint && isVisible() && getUserVisibleHint());
    }

    protected void loadIfVisible() {
        if (!canLoad() || mIsLoaded) {
            return;
        }

        load();
        mIsLoaded = true;
    }

    private static class RemoveBookmarkTask extends UiAsyncTask<Void, Void, Void> {
        private final Context mContext;
        private final int mId;

        public RemoveBookmarkTask(Context context, int id) {
            super(ThreadUtils.getBackgroundHandler());

            mContext = context;
            mId = id;
        }

        @Override
        public Void doInBackground(Void... params) {
            ContentResolver cr = mContext.getContentResolver();
            BrowserDB.removeBookmark(cr, mId);
            return null;
        }

        @Override
        public void onPostExecute(Void result) {
            Toast.makeText(mContext, R.string.bookmark_removed, Toast.LENGTH_SHORT).show();
        }
    }


    private static class RemoveReadingListItemTask extends UiAsyncTask<Void, Void, Void> {
        private final int mId;
        private final String mUrl;
        private final Context mContext;

        public RemoveReadingListItemTask(Context context, int id, String url) {
            super(ThreadUtils.getBackgroundHandler());
            mId = id;
            mUrl = url;
            mContext = context;
        }

        @Override
        public Void doInBackground(Void... params) {
            ContentResolver cr = mContext.getContentResolver();
            BrowserDB.removeReadingListItem(cr, mId);

            GeckoEvent e = GeckoEvent.createBroadcastEvent("Reader:Remove", mUrl);
            GeckoAppShell.sendEventToGecko(e);

            return null;
        }
    }

    private static class RemoveHistoryTask extends UiAsyncTask<Void, Void, Void> {
        private final Context mContext;
        private final int mId;

        public RemoveHistoryTask(Context context, int id) {
            super(ThreadUtils.getBackgroundHandler());

            mContext = context;
            mId = id;
        }

        @Override
        public Void doInBackground(Void... params) {
            BrowserDB.removeHistoryEntry(mContext.getContentResolver(), mId);
            return null;
        }

        @Override
        public void onPostExecute(Void result) {
            Toast.makeText(mContext, R.string.history_removed, Toast.LENGTH_SHORT).show();
        }
    }
}
