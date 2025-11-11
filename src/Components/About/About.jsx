import React from 'react';
import './About.css';
import theme_pattern from '../../assets/theme_pattern.svg';
import profile_img from '../../assets/profile_img.svg';

const About = () => {
    return (
        <div id='about' className='about'>
            <div className="about-title">
                <h1>About me</h1>
                <img src={theme_pattern} alt="" />
            </div>

            <div className="about-section">
                <div className="about-left">
                    <img src={profile_img} alt="Profile" />
                </div>

                <div className="about-right">
                    <div className="about-para">
                        <p>I don’t know what I’m doing</p>
                        <p>Funny thing, right?</p>
                    </div>

                    <div className="about-skills">
                        <div className="about-skills">
                            <div className="about-skills">
                                <div className="about-skill"><p>HTML & CSS</p><hr style={{ width: "90%" }} /></div>
                                <div className="about-skill"><p>React JS</p><hr style={{ width: "80%" }} /></div>
                                <div className="about-skill"><p>JavaScript</p><hr style={{ width: "70%" }} /></div>
                                <div className="about-skill"><p>Next JS</p><hr style={{ width: "60%" }} /></div>
                            </div>
                        </div>
                    </div>
                </div>
            </div>

            <div className="about-achivements">
                <div className="about-achivement">
                    <h1>10+</h1>
                    <p>Years of experience</p>
                </div>
                <hr />
                <div className="about-achivement">
                    <h1>90+</h1>
                    <p>Projects completed</p>
                </div>
                <hr />
                <div className="about-achivement">
                    <h1>15+</h1>
                    <p>Happy clients</p>
                </div>
            </div>
        </div>
    );
};

export default About;
