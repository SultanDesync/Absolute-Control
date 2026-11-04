package acp.ui
{
    public final class PanelLayout
    {
        public static const STAGE_WIDTH:Number = 1920;
        public static const STAGE_HEIGHT:Number = 1080;
        public static const SAFE_MARGIN:Number = 56;
        public static const SAFE_WIDTH:Number = 1808;
        public static const SAFE_HEIGHT:Number = 968;

        public static const HEADER_X:Number = SAFE_MARGIN;
        public static const HEADER_Y:Number = SAFE_MARGIN;
        public static const HEADER_WIDTH:Number = SAFE_WIDTH;
        public static const HEADER_HEIGHT:Number = 48;

        public static const SIDEBAR_X:Number = SAFE_MARGIN;
        public static const SIDEBAR_HEADER_Y:Number = 116;
        public static const SIDEBAR_Y:Number = 124;
        public static const SIDEBAR_WIDTH:Number = 340;
        public static const SIDEBAR_HEIGHT:Number = 836;
        public static const SIDEBAR_GROUP_HEIGHT:Number = 34;
        public static const SIDEBAR_MODULE_HEIGHT:Number = 44;
        public static const SIDEBAR_PAGE_HEIGHT:Number = 42;
        public static const SIDEBAR_SECTION_HEIGHT:Number = 34;
        public static const SIDEBAR_GAP:Number = 4;

        public static const WORKSPACE_X:Number = 416;
        public static const WORKSPACE_WIDTH:Number = 1448;
        public static const SCROLL_RAIL_WIDTH:Number = 52;
        public static const CONTROL_ROW_WIDTH:Number =
            WORKSPACE_WIDTH - SCROLL_RAIL_WIDTH;

        public static const ROWS_Y:Number = 164;
        public static const ROW_HEIGHT:Number = 52;
        public static const VISIBLE_ROWS:int = 13;
        public static const ROWS_BOTTOM:Number = 856;
        public static const HELP_Y:Number = 864;
        public static const HELP_HEIGHT:Number = 104;
        public static const FOOTER_X:Number = SAFE_MARGIN;
        public static const FOOTER_Y:Number = 976;
        public static const FOOTER_WIDTH:Number = SAFE_WIDTH;
        public static const FOOTER_HEIGHT:Number = 48;
        public static const FOOTER_ACTIONS_X:Number = WORKSPACE_X - FOOTER_X;
        public static const SLIDER_TRACK_X:Number = 24;
        public static const SLIDER_TRACK_WIDTH:Number = 584;
        public static const SLIDER_THUMB_RADIUS:Number = 10;
        public static const SLIDER_VALUE_X:Number = 652;
        public static const SLIDER_VALUE_WIDTH:Number = 86;
        public static const CHOICE_POPUP_WIDTH:Number = 478;
        public static const CHOICE_OPTION_HEIGHT:Number = 38;
        public static const VISIBLE_CHOICE_OPTIONS:int = 8;

        public static const FOCUS_MODULES:int = 0;
        public static const FOCUS_CONTROLS:int = 1;
        public static const FOCUS_ACTIONS:int = 2;
        public static const FOCUS_GRID:int = 3;
        public static const FOCUS_ANCHORS:int = 4;
    }
}
