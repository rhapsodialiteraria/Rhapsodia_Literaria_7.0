import React, { useState, useEffect } from "react";
import "./HostImages.css";

// Use actual images from your assets folder
import bg1 from "../../assets/Background.svg";
import bg2 from "../../assets/Background2.svg";
import bg3 from "../../assets/Background.svg";

const images = [bg1, bg2, bg3];

const HostImages = () => {
    const [currentIndex, setCurrentIndex] = useState(0);

    // Auto-change every 3 seconds
    useEffect(() => {
        const timer = setTimeout(() => {
            setCurrentIndex((prev) => (prev + 1) % images.length);
        }, 3000);
        return () => clearTimeout(timer);
    }, [currentIndex]);

    return (
        <div id="home" className="host-container">
            {images.map((img, index) => (
                <div
                    key={index}
                    className={`background-slide ${index === currentIndex ? "active" : ""
                        }`}
                    style={{
                        backgroundImage: `url(${img})`,
                    }}
                ></div>
            ))}

            <div className="host-dots">
                {images.map((_, index) => (
                    <span
                        key={index}
                        className={`dot ${currentIndex === index ? "active" : ""}`}
                        onClick={() => setCurrentIndex(index)}
                    ></span>
                ))}
            </div>
        </div>
    );
};

export default HostImages;
