import React, { useState } from 'react';
import type { ViewStyle } from 'react-native';
import { Button, ScrollView, StyleSheet, View } from 'react-native';
import type { WithSpringConfig } from 'react-native-reanimated';
import Animated, {
  useAnimatedStyle,
  useSharedValue,
  withSpring,
} from 'react-native-reanimated';

const VIOLET = '#b58df1';
const BORDER_WIDTH = 4;
const FRAME_WIDTH = 350;
const CLAMP_MARKER_HEIGHT = 20;
const CLAMP_MARKER_WIDTH = 50;
const CLAMP_MARKER_OFFSET = 20;

const LOWER_SPRING_TO_VALUE = CLAMP_MARKER_WIDTH + CLAMP_MARKER_OFFSET;
const UPPER_SPRING_TO_VALUE =
  FRAME_WIDTH - (CLAMP_MARKER_WIDTH + CLAMP_MARKER_OFFSET);

function Visualiser({
  testedStyle,
}: {
  testedStyle: ViewStyle;
  description: string;
}) {
  return (
    <>
      <View
        style={{
          width: FRAME_WIDTH + 2 * BORDER_WIDTH,
          borderWidth: BORDER_WIDTH,
          borderColor: VIOLET,
        }}>
        <View>
          <View
            style={[
              styles.toValueMarker,
              {
                width: LOWER_SPRING_TO_VALUE,
              },
            ]}
          />
          <View
            style={[
              styles.clampMarker,
              {
                width: CLAMP_MARKER_WIDTH,
              },
            ]}
          />
        </View>
        <Animated.View style={[styles.movingBox, testedStyle]} />
        <View>
          <View
            style={[
              styles.toValueMarker,
              {
                marginTop: -CLAMP_MARKER_HEIGHT / 2,
                width: LOWER_SPRING_TO_VALUE,
                alignSelf: 'flex-end',
              },
            ]}
          />
          <View
            style={[
              styles.clampMarker,
              {
                marginTop: -50,
                width: CLAMP_MARKER_WIDTH,
                alignSelf: 'flex-end',
              },
            ]}
          />
        </View>
      </View>
    </>
  );
}

export default function SpringOldConfigExample() {
  const lowWidth = useSharedValue(LOWER_SPRING_TO_VALUE * 3);
  const mediumWidth = useSharedValue(LOWER_SPRING_TO_VALUE * 2);
  const highWidth = useSharedValue(LOWER_SPRING_TO_VALUE);
  const [widthToggle, setWidthToggle] = useState(false);

  const oldDefaultConfig: WithSpringConfig = {
    damping: 10,
    mass: 1,
    stiffness: 100,
  };

  const oldDefaultConfigLowStyle = useAnimatedStyle(() => {
    return {
      width: withSpring(lowWidth.value / 3, oldDefaultConfig),
    };
  });

  const oldDefaultConfigMediumStyle = useAnimatedStyle(() => {
    return {
      width: withSpring(mediumWidth.value / 2, oldDefaultConfig),
    };
  });

  const oldDefaultConfigHighStyle = useAnimatedStyle(() => {
    return {
      width: withSpring(highWidth.value, oldDefaultConfig),
    };
  });

  return (
    <ScrollView style={styles.container} contentContainerStyle={styles.content}>
      <Visualiser
        testedStyle={oldDefaultConfigLowStyle}
        description="Old default config, no timing, low value"
      />
      <Visualiser
        testedStyle={oldDefaultConfigMediumStyle}
        description="Old default config, no timing, medium value"
      />
      <Visualiser
        testedStyle={oldDefaultConfigHighStyle}
        description="Old default config, no timing, big value"
      />
      <Button
        title="toggle"
        onPress={() => {
          lowWidth.value = widthToggle
            ? LOWER_SPRING_TO_VALUE * 3
            : UPPER_SPRING_TO_VALUE;

          mediumWidth.value = widthToggle
            ? LOWER_SPRING_TO_VALUE * 2
            : UPPER_SPRING_TO_VALUE;

          highWidth.value = widthToggle
            ? LOWER_SPRING_TO_VALUE
            : UPPER_SPRING_TO_VALUE;

          setWidthToggle(!widthToggle);
        }}
      />
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: {
    paddingTop: '21.37%',
    flex: 1,
    flexDirection: 'column',
    padding: CLAMP_MARKER_HEIGHT,
    paddingBottom: '21.37%',
  },
  content: {
    alignItems: 'center',
    paddingBottom: 100,
  },
  toValueMarker: {
    position: 'absolute',
    margin: 0,
    opacity: 1,
    zIndex: 100,
    height: CLAMP_MARKER_HEIGHT / 2,
    backgroundColor: VIOLET,
  },
  clampMarker: {
    position: 'absolute',
    margin: 0,
    opacity: 0.5,
    height: 50,
    backgroundColor: VIOLET,
  },
  movingBox: {
    zIndex: 1,
    height: 50,
    opacity: 0.5,
    backgroundColor: 'black',
  },
});
