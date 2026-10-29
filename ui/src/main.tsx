import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
// Latin subset only: the whole UI is inlined into one HTML file served by the plugin.
import "@fontsource/inter/latin-400.css";
import "@fontsource/inter/latin-500.css";
import "@fontsource/inter/latin-600.css";
import "@fontsource/inter/latin-700.css";
import "./styles/tokens.css";
import "./styles/layout.css";
import "./styles/components.css";
import App from "./App";
import { connect } from "./store";

void connect();

createRoot(document.getElementById("root")!).render(
  <StrictMode>
    <App />
  </StrictMode>,
);
