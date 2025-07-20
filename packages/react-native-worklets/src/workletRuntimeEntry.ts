// @ts-nocheck

'use strict';

import { init } from './initializers';
import { WorkletsError } from './WorkletsError';

/**
 * This function is an entry point for Worklet Runtimes. We can use it to setup
 * necessary tools, like the ValueUnpacker.
 *
 * We must throw an error at the end of this function to prevent the bundle to
 * continue executing. This is because the next module to be ran would be the
 * React Native one, and it would break the Worklet Runtime if initialized. The
 * error is caught in C++ code.
 *
 * This function has no effect on the RN Runtime beside setting the
 * `_WORKLETS_BUNDLE_MODE` flag.
 */
export function bundleModeInit() {
  globalThis._WORKLETS_BUNDLE_MODE = true;
  if (globalThis._WORKLET) {
    /**
     * We shouldn't call `init()` on RN Runtime here, as it would initialize our
     * module before React Native has configured the RN Runtime.
     */
    init();

    if (__DEV__) {
      // @ts-expect-error type not exposed by Metro
      const modules = require.getModules();
      // @ts-expect-error type not exposed by Metro
      const ReactNativeModuleId = require.resolveWeak('react-native');
      globalThis._log("overriding 'react-native' module");

      const factory = function (
        _global: unknown,
        _$$_REQUIRE: unknown,
        _$$_IMPORT_DEFAULT: unknown,
        _$$_IMPORT_ALL: unknown,
        module: Record<string, unknown>,
        _exports: unknown,
        _dependencyMap: unknown
      ) {
        module.exports = new Proxy(
          {},
          {
            get(_target, prop) {
              globalThis.console.warn(
                `You tried to import '${String(prop)}' from 'react-native' module on a Worklet Runtime. Using 'react-native' module on a Worklet Runtime is not allowed.`
              );
              return {
                get() {
                  return undefined;
                },
              };
            },
          }
        );
      };

      const mod = {
        dependencyMap: [],
        factory,
        hasError: false,
        importedAll: {},
        importedDefault: {},
        isInitialized: false,
        publicModule: {
          exports: {},
        },
      };

      modules.set(ReactNativeModuleId, mod);

      const Refresh = new Proxy(
        {},
        {
          get() {
            return () => {};
          },
        }
      );

      // @ts-expect-error type not exposed by Metro
      globalThis.__r.Refresh = Refresh;

      const PolyfillFunctionsId = require.resolveWeak(
        'react-native/Libraries/Utilities/PolyfillFunctions'
      );

      const polyfillFactory = function (
        _global: unknown,
        _$$_REQUIRE: unknown,
        _$$_IMPORT_DEFAULT: unknown,
        _$$_IMPORT_ALL: unknown,
        module: Record<string, unknown>,
        _exports: unknown,
        _dependencyMap: unknown
      ) {
        module.exports.polyfillGlobal = (
          name: string,
          getValue: () => unknown
        ) => {
          globalThis._log('polyfillGlobal ' + name + ' ' + getValue);
          globalThis[name] = getValue();
        };
      };

      const polyfillMod = {
        dependencyMap: [],
        factory: polyfillFactory,
        hasError: false,
        importedAll: {},
        importedDefault: {},
        isInitialized: false,
        publicModule: {
          exports: {},
        },
      };

      modules.set(PolyfillFunctionsId, polyfillMod);

      const TurboModuleRegistryId = require.resolveWeak(
        'react-native/Libraries/TurboModule/TurboModuleRegistry'
      );

      const TurboModules = new Map<string, unknown>();

      // globalThis.TurboModules = new Map<string, unknown>();
      globalThis.TurboModules = TurboModules;

      TurboModules.set('Networking', {});

      const faactory = function (
        _global: unknown,
        _$$_REQUIRE: unknown,
        _$$_IMPORT_DEFAULT: unknown,
        _$$_IMPORT_ALL: unknown,
        module: Record<string, unknown>,
        _exports: unknown,
        _dependencyMap: unknown
      ) {
        function get(name: string) {
          globalThis._log('TurboModuleRegistry get ' + name);
          return globalThis.TurboModules.get(name);
        }
        function getEnforcing(name: string) {
          globalThis._log('TurboModuleRegistry getEnforcing ' + name);
          return globalThis.TurboModules.get(name);
        }
        module.exports.get = get;
        module.exports.getEnforcing = getEnforcing;
      };

      const mood = {
        dependencyMap: [],
        factory: faactory,
        hasError: false,
        importedAll: {},
        importedDefault: {},
        isInitialized: false,
        publicModule: {
          exports: {},
        },
      };

      modules.set(TurboModuleRegistryId, mood);
    }

    throw new WorkletsError('Worklets initialized successfully');
  }
}

bundleModeInit();
