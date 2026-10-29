import type { SVGProps } from "react";

type IconProps = SVGProps<SVGSVGElement> & { size?: number };

function Svg({ size = 18, children, ...rest }: IconProps) {
  return (
    <svg
      width={size}
      height={size}
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth={2}
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
      {...rest}
    >
      {children}
    </svg>
  );
}

export const PlayIcon = (p: IconProps) => (
  <Svg {...p}>
    <path d="M7 4.5v15l12.5-7.5z" fill="currentColor" stroke="none" />
  </Svg>
);

export const StopIcon = (p: IconProps) => (
  <Svg {...p}>
    <rect x="6" y="6" width="12" height="12" rx="2" fill="currentColor" stroke="none" />
  </Svg>
);

/** Padlock whose shackle drops shut when locked (animated by CSS). */
export function LockIcon({ locked, size = 14 }: { locked: boolean; size?: number }) {
  return (
    <svg width={size} height={size} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth={2.2}
      strokeLinecap="round" strokeLinejoin="round" aria-hidden="true" className={`lock-icon ${locked ? "is-locked" : ""}`}>
      <path className="lock-icon__shackle" d="M7.5 11V7.5a4.5 4.5 0 0 1 9 0V11" />
      <rect x="4.5" y="11" width="15" height="10" rx="2.5" />
      <circle cx="12" cy="16" r="1.4" fill="currentColor" stroke="none" />
    </svg>
  );
}

export const LoopIcon = (p: IconProps) => (
  <Svg {...p}>
    <path d="M17 2.5l3 3-3 3" />
    <path d="M4 11v-1.5a4 4 0 0 1 4-4h12" />
    <path d="M7 21.5l-3-3 3-3" />
    <path d="M20 13v1.5a4 4 0 0 1-4 4H4" />
  </Svg>
);

export const ResetIcon = (p: IconProps) => (
  <Svg {...p}>
    <path d="M3.5 12a8.5 8.5 0 1 0 2.6-6.1" />
    <path d="M3 3.5v4.5h4.5" />
  </Svg>
);

export const ChevronLeft = (p: IconProps) => (
  <Svg {...p}>
    <path d="M15 5l-7 7 7 7" />
  </Svg>
);

export const ChevronRight = (p: IconProps) => (
  <Svg {...p}>
    <path d="M9 5l7 7-7 7" />
  </Svg>
);

export const ChevronDown = (p: IconProps) => (
  <Svg {...p}>
    <path d="M6 9l6 6 6-6" />
  </Svg>
);

export const SavePresetIcon = (p: IconProps) => (
  <Svg {...p}>
    <path d="M14 3H6.5A2.5 2.5 0 0 0 4 5.5v13A2.5 2.5 0 0 0 6.5 21H12" />
    <path d="M14 3l5 5v4" />
    <path d="M14 3v5h5" />
    <path d="M8 12.5h6M8 16.5h3.5" />
    <path d="M18 15v6M15 18h6" />
  </Svg>
);

export const MenuIcon = (p: IconProps) => (
  <Svg {...p}>
    <path d="M4 6.5h16M4 12h16M4 17.5h16" />
  </Svg>
);

export const MoonIcon = (p: IconProps) => (
  <Svg {...p}>
    <path d="M20 14.5A8 8 0 0 1 9.5 4a8 8 0 1 0 10.5 10.5z" />
  </Svg>
);

export const SunIcon = (p: IconProps) => (
  <Svg {...p}>
    <circle cx="12" cy="12" r="4" />
    <path d="M12 2.5v2M12 19.5v2M2.5 12h2M19.5 12h2M5.3 5.3l1.4 1.4M17.3 17.3l1.4 1.4M5.3 18.7l1.4-1.4M17.3 6.7l1.4-1.4" />
  </Svg>
);

export const HeadphonesIcon = (p: IconProps) => (
  <Svg {...p}>
    <path d="M3.5 17v-4.5a8.5 8.5 0 0 1 17 0V17" />
    <rect x="3" y="14" width="4.5" height="7" rx="1.6" fill="currentColor" />
    <rect x="16.5" y="14" width="4.5" height="7" rx="1.6" fill="currentColor" />
  </Svg>
);

export const PowerIcon = (p: IconProps) => (
  <Svg {...p}>
    <path d="M12 3v8.5" />
    <path d="M6.2 6.6a8 8 0 1 0 11.6 0" />
  </Svg>
);

export const CheckIcon = (p: IconProps) => (
  <Svg {...p}>
    <path d="M5 12.5l4.5 4.5L19 7.5" />
  </Svg>
);

export const FolderIcon = (p: IconProps) => (
  <Svg {...p}>
    <path d="M3.5 7.5a2 2 0 0 1 2-2h4l2 2.5h7a2 2 0 0 1 2 2V17a2 2 0 0 1-2 2h-13a2 2 0 0 1-2-2z" />
  </Svg>
);

export const DownloadIcon = (p: IconProps) => (
  <Svg {...p}>
    <path d="M12 4v11" />
    <path d="M7.5 10.5L12 15l4.5-4.5" />
    <path d="M4.5 19.5h15" />
  </Svg>
);

export const OpenFileIcon = (p: IconProps) => (
  <Svg {...p}>
    <path d="M9 18V6.5l10-2V16" />
    <circle cx="6.5" cy="18" r="2.5" />
    <circle cx="16.5" cy="16" r="2.5" />
  </Svg>
);

/** Path of the Glacier Riser mark (a spiral), in its own -60 -60 1173 1172 viewBox. */
export const BRAND_MARK_PATH =
  "M633 547C634 585 623 610 597 638C534 706 433 689 372 627C296 549 308 419 383 344C429 300 482 275 546 276C695 276 813 416 810 560C808 648 771 724 708 784C635 854 546 885 446 884C363 883 283 854 219 804C4 637 8 328 197 144C289 54 414 4 543 6C677 8 805 64 898 161C996 263 1049 391 1046 533C1043 668 987 797 890 891C781 996 639 1048 488 1038C436 1034 372 1022 323 1003L317 1001L317 999L326 1001C360 1007 390 1011 425 1012C535 1016 643 983 732 920C829 852 902 750 922 632C942 521 916 407 851 315C794 234 707 179 610 161C519 145 416 165 340 219C267 270 218 347 203 434C175 600 281 757 448 786C516 798 586 783 641 743C688 709 720 657 730 600C738 547 728 499 697 456C668 415 626 383 575 376C502 365 432 408 421 484C411 547 448 611 514 621C542 626 577 614 600 598C618 584 625 567 633 547Z";

/** The Glacier Riser mark, filled with the theme's logo gradient (--logo-a / --logo-b). */
export function BrandLogo({ size = 38 }: { size?: number }) {
  return (
    <svg width={size} height={size} viewBox="-60 -60 1173 1172" aria-hidden="true">
      <defs>
        <linearGradient id="logoGrad" x1="0" y1="0" x2="1" y2="1">
          <stop offset="0" style={{ stopColor: "var(--logo-a)" }} />
          <stop offset="1" style={{ stopColor: "var(--logo-b)" }} />
        </linearGradient>
      </defs>
      <path d={BRAND_MARK_PATH} fill="url(#logoGrad)" />
    </svg>
  );
}
