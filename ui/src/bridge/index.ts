import { createJuceBackend, isRunningInJuce } from "./juceBackend";
import { createMockBackend } from "./mockBackend";
import type { Backend } from "./types";

/** Inside the plugin we talk to C++; in a plain browser we use the mock. */
export const backend: Backend = isRunningInJuce() ? createJuceBackend() : createMockBackend();

export * from "./types";
